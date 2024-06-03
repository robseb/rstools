
/**
 *  
 * @file    main.cpp
 * @brief   FPGA-status
 * @author  rsyocto GmbH & Co. KG 
 * 			Robin Sebastian (git@robseb.de)
 * @mainpage
 * rstools application to write to any HSP-to-FPGA Bridges or the MPU address space
 * 
 * Change Log: 
 * 		1.00 (12-07-2019)
 * 			Initial release
 * 		1.10 (03-05-2022)
 * 			GPO Mode and updated design
 * 		1.11 (03-14-2022)
 * 			Bug fix of writing to POSIX I/O
 * 
 * Copyright (C) 2020-2022 rsyocto GmbH & Co. KG  *  All Rights Reserved
 * 
 */

#define VERSION "1.10"

#include <cstdio>
#include <iostream>
#include <fstream>					// POSIX: for acessing Linux drivers
#include <sys/mman.h>				// POSIX: memory maping
#include <fcntl.h>					// POSIX: "PROT_WRITE", "MAP_SHARED", ...
#include <unistd.h>					// POSIX: for closing the Linux driver access
#include <cstdint>                  // Standard integral types (uint8_t,...)
#include <thread>					// Required for putting task to sleep 
#include <chrono>					// Required for putting task to sleep 
#include <sstream>
#include <cstdint>                  // Standard integral types (uint8_t,...)
using namespace std;

#define DEC_INPUT 1
#define HEX_INPUT 0
#define BIN_INPUT 2

// Bridge Interfaces Base addresses 
#define LWHPSFPGA_OFST  0xff200000 	// LWHPS2FPGA Bridge 
#define HPSFPGA_OFST    0xC0000000 	// HPS2FPGA Bridge 
#define MPU_OFSET		0x0        	// MPU (HPS Address space)

#define FPGAMAN_GPO_OFST    0xFF706010

// Bridge interface End address 
#define LWHPSFPGA_END   0xFF3FFFFF
#define HPSFPGA_END     0xFBFFFFFF
#define MPU_END         0xFFFFFFFF

// Bridge interface range (allowed input offset)
#define LWH2F_RANGE    (LWHPSFPGA_END - LWHPSFPGA_OFST)
#define H2F_RANGE      (HPSFPGA_END - HPSFPGA_OFST)
#define MPU_RANGE      (MPU_END - MPU_OFSET)

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

/*
*	@param	Check that the Input is a valid HEX or DEC String
*   @param  input 		String to check
*   @param  DecHex		True  ==> DEC Mode
*   					False ==> HEX Mode
*	@return is Valid
*/
bool checkIfInputIsVailed(std::string input, bool DecHex)
{
	if (input.length() < 1) return false;
	uint16_t i = 0;

	// remove suffix "0x"
	if ((input.find_first_of("0x", 0) == 0) && (!DecHex))
	{
		input.replace(0, 2, "");
	}

	for (i = 0; i < input.length(); i++)
	{
		if (i != input.find_first_of(DecHex ? "0123456789" : "0123456789abcdefABCDEF", i)) break;
	}

	if (i == input.length()) return true;

	return false;
}


int main(int argc, const char* argv[])
{
	// Debugging Test values 
	//argv[1] = "-hf";
	//argv[2] =(const char*) "0"; // Address 
	//argv[3] = (const char*)"123"; // Value
	//argc = 3;

	
	//argv[1] = "-lw";
	//argv[2] =(const char*) "0"; // Address 
	//argv[3] ="-b";
	//argv[4] = (const char*)"31"; // Bit Pos
	//argv[5] = (const char*)"0";  // Bit Set
	//argc = 5;
	
	// Read to the Light Wightweight or AXI HPS to FPGA Interface
	if (((argc > 3) && (std::string(argv[1]) == "-lw"))  || ((argc > 3) && (std::string(argv[1]) == "-hf"))|| \
	    ((argc > 3) && (std::string(argv[1]) == "-mpu")) || ((argc > 1) && (std::string(argv[1]) == "-gpo")))
	{
		// read the selected Bridge Interface 
		bool lwBdrige 		= false;
		bool gpo_write_mode = false;
		uint8_t arg_no		=0;
		uint32_t address;
		
		uint8_t address_space = 0; // 0: HPS2FPGA | 1: LWHPS2FPGA | 2: MPU
		if (std::string(argv[1]) == "-lw")
			lwBdrige = true;
		else if (std::string(argv[1]) == "-mpu")
			address_space = 2;

		else  if(std::string(argv[1]) == "-gpo")
		{	
			// Enable writing GPO (HPS->FPGA) Register 
			// Using MPU mode with fixed address 
			gpo_write_mode = true;
			address_space = 2;
			address = FPGAMAN_GPO_OFST;
			arg_no =1;
		}

		/// check the value input type (Dec or Hex) ///
		// 1: DEC Value input | 0: HEX Dec Input | 2: Binary Bit Set/Reset 
		int DecHexBin = DEC_INPUT;
		bool ConsloeOutput = true;

		// check the Value input type (dec,hex,bin)
		if ((argc >= 4-arg_no) && (std::string(argv[3-arg_no]) == "-h"))
			DecHexBin = HEX_INPUT;

		if ((argc > 4-arg_no) && (std::string(argv[3-arg_no]) == "-b"))
			DecHexBin = BIN_INPUT;

		std::string ValueString;

		switch (DecHexBin)
		{
		case DEC_INPUT:
			if ((argc > 4-arg_no) && (std::string(argv[4-arg_no]) == "-b"))
				ConsloeOutput = false;
			ValueString = argv[3-arg_no];
			break;
		case HEX_INPUT:
			if ((argc > 5-arg_no) && (std::string(argv[5-arg_no]) == "-b"))
				ConsloeOutput = false;
			ValueString = argv[4-arg_no];
			break;
		case BIN_INPUT:
			if ((argc > 6-arg_no) && (std::string(argv[6-arg_no]) == "-b"))
				ConsloeOutput = false;
			break;
		default:
			break;
		}

		uint32_t ValueInput = 0;
		uint64_t ValueInputTemp = ValueInput;
		bool InputVailed = true;
		uint32_t BitPosValue = 0;
		uint32_t SetResetBit = 0;
		uint32_t addressOffset = 0;
		std::string BinValueStr="";

		/// Check Address Input ///
		if(!gpo_write_mode)
		{
			std::string AddresshexString =argv[2];
		
			// check if the address hex input is vailed
			
			if (checkIfInputIsVailed(AddresshexString, false))
			{
				istringstream buffer(AddresshexString);
				buffer >> hex >> addressOffset;

				// Address must be a 32-bit address
				if (addressOffset % 4 >0)
				{
					cout << "[ ERROR ]  The Address 0x"<<hex<<addressOffset<<" is not not a 32-bit Address" <<endl;
					cout << "           Use the next lower address: 0x"<<(addressOffset-(addressOffset%4))<<dec<<endl;
					InputVailed = false;
				}

				// HPS2FPGA
				if (address_space == 0)
				{
					// check the range of the AXI HPS-to-FPGA Bridge Interface 
					if (addressOffset > H2F_RANGE)
					{
						if (ConsloeOutput)
							cout << "[  ERROR  ] Selected Address is outside of the HPS to "\
							"FPGA AXI Bridge Range!" << endl;
						InputVailed = false;
					}
				}
				// LWHPS2FPGA
				else if (address_space == 1)
				{
					// check the range of the Lightweight HPS-to-FPGA Bridge Interface 
					if (addressOffset > LWH2F_RANGE)
					{
						if (ConsloeOutput)
							cout << "[  ERROR  ] Selected Address is outside of"\
							"the Lightweight HPS-to-FPGA Bridge Range!" << endl;
						InputVailed = false;
					}
				}
				// MPU
				else
				{
					// check the range of the MPU address space
					if (addressOffset > MPU_RANGE)
					{
						if (ConsloeOutput)
							cout << "[  ERROR  ] RROR: selected address is outside of"\
						"the HPS Address range!" << endl;
						InputVailed = false;
					}
				}
			}
			else
			{
				// address input is not vailed
				if (ConsloeOutput)
					cout << "[  ERROR  ]  Selected Address Input is not a HEX Address!" << endl;
				InputVailed = false;
			}
		}

		// only for binary mode: check if the Set or Reset input is vailed //
		if (DecHexBin == BIN_INPUT)
		{
			// read and check the Pos input value
			std::string SetInputString = argv[5-arg_no];
			std::string BitPosString = argv[4-arg_no];
			

			// check if the Bit pos value input is okay
			if (checkIfInputIsVailed(BitPosString, true))
			{
				istringstream buffer(BitPosString);
				buffer >> BitPosValue;

				if (BitPosValue > 32)
					InputVailed = false;
			}
			else 
				InputVailed = false;
			// read and check the Set or Reset input
			if (InputVailed && checkIfInputIsVailed(SetInputString, true))
			{
				istringstream buffer(SetInputString);
				buffer >> SetResetBit;

				if (!(SetResetBit==1 || SetResetBit==0))
					InputVailed = false;
			}
			else 
				InputVailed = false;

			if (SetResetBit==1)	BinValueStr ="|=  (1<<"+BitPosString+")";
			else				BinValueStr ="&= ~(1<<"+BitPosString+")";

		}
		else
		{
			// for DEC and HEX
			// check if the address hex input is vailed

			if (checkIfInputIsVailed(ValueString, !(DecHexBin == HEX_INPUT)))
			{
				istringstream buffer(ValueString);

				if (DecHexBin == DEC_INPUT)
					buffer >> dec >> ValueInputTemp;
				else
					buffer >> hex >> ValueInputTemp;

				// value a 32 Bit value 
				if (ValueInputTemp > UINT32_MAX)
				{
					if (ConsloeOutput)
						cout << "[  ERROR  ] Selected value greater than 32 bits" << endl;
					InputVailed = false;
				}

				ValueInput = ValueInputTemp;
			}
			else
			{
				// value input is not vailed
				if (ConsloeOutput)
					cout << "[  ERROR  ] Selected Value is Input is not vailed!" << endl;
				InputVailed = false;
			}
		}
		if (address_space < 2)
			address = (lwBdrige ? LWHPSFPGA_OFST : HPSFPGA_OFST) + addressOffset;
		else
			address = addressOffset;
		

		// only in case the input is vailed write the request to the light wight bus
		if (InputVailed)
		{
			if (ConsloeOutput)
			{
				cout << "------------------------------------WRITING------------------------------------------" << endl;
				if (address_space < 2)
				{
					cout << "   Bridge:      " << (lwBdrige ? "Lightweight HPS-to-FPGA" : "HPS-to-FPGA");
					cout << "      Brige Base:  0x" << hex << (lwBdrige ? LWHPSFPGA_OFST : HPSFPGA_OFST) << dec << endl;
					cout << "   Your Offset: 0x" << hex << addressOffset << dec;
					cout << "      Address:  0x" << hex << address << dec << endl;
					if (DecHexBin == BIN_INPUT)
						cout << "   Value:       " << BinValueStr<<endl;
					else
						cout << "   Value:       " << ValueInput <<hex<<" [0x"<< ValueInput<<"]"<<dec<<endl;
						
				}
				else 
				{	
					if (!gpo_write_mode)
					{
						cout << "   Brige Base:  0x00 (MPU Address Space)"<< endl;
						cout << "   Address:     0x" << hex << address << dec << endl;
						if (DecHexBin == BIN_INPUT)
							cout << "   Value:       " << BinValueStr<<endl;
						else
							cout << "   Value:       " << ValueInput <<hex<<" [0x"<< ValueInput<<"]"<<dec<<endl;
					}
					else
					{
						cout << "   Brige Base: 32-bit GPO (General-Purpose Output Register) HPS->FPGA " << endl;
						cout << "   Address:     0x" << hex << FPGAMAN_GPO_OFST << dec << endl;
						if (DecHexBin == BIN_INPUT)
							cout << "   Value:       " << BinValueStr<<endl;
						else
							cout << "   Value:       " << ValueInput <<hex<<" [0x"<< ValueInput<<"]"<<dec<<endl;
					}
				}
			}
			do
			{
				void* bridgeMap;
				int fd;

				// open memory driver 
				fd = open("/dev/mem", (O_RDWR | O_SYNC));

				// was opening okay
				if (fd < 0)
				{
					if (ConsloeOutput)
						cout << "ERROR: Failed to open memory driver!" << endl;
					else
						cout << -2;
					break;
				}

				// configure a virtual memory interface to the bridge or mpu
				bridgeMap = mmap(NULL, MAP_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fd, \
					address & ~MAP_MASK);

				// check if opening was sucsessful
				if (bridgeMap == MAP_FAILED)
				{
					if (ConsloeOutput)
						cout << "ERROR: Accesing the virtual memory failed!" << endl;
					else
						cout << -2;
					close(fd);
					return 0;
				}

				// access to Bridge is okay 
				// write the value to the address 

				uint16_t delay_count = 0;
				uint32_t* ptrmap = (uint32_t*)(bridgeMap +(address & MAP_MASK));
				// print also the old value of the selected register
				if (ConsloeOutput)
				{
					cout << "   old Value:   " << *ptrmap << " [0x" << hex << *ptrmap << "]" << dec << endl;
				}

				// write the value to the address 

				// write the new value to the selected register
				
				if (DecHexBin == BIN_INPUT)
				{
					if (SetResetBit) *ptrmap |=  (1 << BitPosValue);
					else			 *ptrmap &= ~(1 << BitPosValue);
				}
				else
					*ptrmap = ValueInput;
				

				// Close the MAP 
				if (munmap(bridgeMap, MAP_SIZE) < 0)
				{
					if (ConsloeOutput)
						cout << "[ ERROR ] Closing of shared memory failed!" << endl;
						else cout << -2;
				}
			

				// close the driver port 
				close(fd);

				if (ConsloeOutput)
					cout << "[  INFO  ]  Writing was successful " << endl;
				else
					cout << 1;

			} while (0);
		}
		else
		{
			// User input is not okay 
			if (!ConsloeOutput)
				cout << -1;
			else
			{
				cout << "[ ERROR ] User Input is wrong!"<<endl;
				cout << "          FPGA-writeBridge -lw|hf|mpu| <offset address in hex>" << endl;
				cout << "                           -h|-b|<value dec> <value hex>|<bit pos> <bit value>  -b " << endl;
				cout << "          FPGA-writeBridge -gpo -h|-b|<value dec> <value hex>|<bit pos> <bit value>  -b" << endl;
			}
		}
	}
	else
	{
		// help output 
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "|        Command to write a 32-bit register to a HPS-to-FPGA Bridge Interface                |" << endl;			
		cout << "|                    or to the entire MPU (HPS) Memory space                                 |" << endl;
		cout << "|                         Designed for Intel SoC FPGAs                                       |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "|$ FPGA-writeBridge -lw [Address Offset in HEX] [Value in DEC]                               |" << endl;
		cout << "|      L   Writing a 32-bit to a Lightweight HPS-to-FPGA Bridge Register in DEC              |" << endl;
		cout << "|          e.g.: FPGA-writeBridge -lw 0A   10                                                |" << endl;
		cout << "|$ FPGA-writeBridge -lw [Address Offset in HEX] -h [Value in HEX]                            |" << endl;
		cout << "|      L   Writing a 32-bit to a Lightweight HPS-to-FPGA Bridge Register in HEX              |" << endl;
		cout << "|          e.g.: FPGA-writeBridge -lw 0A  -h abab                                            |" << endl;
		cout << "|$ FPGA-writeBridge -lw [Address Offset in HEX] -b [Bit Pos] [Bit Value]                     |" << endl;
		cout << "|      L   Setting a 1-bit of a 32-bit Register to a Lightweight HPS-to-FPGA Bridge Register |" << endl;
		cout << "|          e.g.: FPGA-writeBridge -lw 0A -b 3 1                                              |" << endl;
		cout << "|$ FPGA-writeBridge -hf [Address Offset in HEX] [Value in DEC]                               |" << endl;
		cout << "|      L    Writing a 32-bit to a HPS-to-FPGA AXI Bridge Register                            |" << endl;
		cout << "|          e.g.: FPGA-writeBridge -hf 8C 128                                                 |" << endl;
		cout << "|$ FPGA-writeBridge -gpo [Value in DEC]                                                      |" << endl;
		cout << "|      L   Writing a 32-bit to the 32-bit GPO (General-Purpose Ouput Register)               |" << endl;
		cout << "|                HPS->FPGA Register                                                          |" << endl;
		cout << "|          e.g.: FPGA-writeBridge -gpo 123                                                   |" << endl;
		cout << "|$ FPGA-writeBridge -mpu [Address Offset in HEX] [Value in DEC]                              |" << endl;
		cout << "|      L   Writing a 32-bit Register of the entire MPU (HPS) memory space                    |" << endl;
		cout << "|          e.g.: FPGA-writeBridge -mpu 0xFFD04000 145                                        |" << endl;
		cout << "|                                                                                            |" << endl;
		cout << "|      Suffix: -b -> only decimal result output                                              |" << endl;
		cout << "|                     L  1 = Written successfully                                            |" << endl;
		cout << "|                     L -1 = Input Error                                                     |" << endl;
		cout << "|                     L -2 = Linux Kernel Memory Driver Error                                |" << endl;
		cout << "|$ FPGA-writeBridge -lw|hf|mpu| <offset address in hex>                                      |" << endl;
		cout << "|                       -h|-b|<value dec> <value hex>|<bit pos> <bit value>  -b              |" << endl;
		cout << "|$ FPGA-writeBridge -gpo -h|-b|<value dec> <value hex>|<bit pos> <bit value>  -b             |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "| Vers.: "<<VERSION<<"                                                                                |"<<endl;
		cout << "| Copyright (C) 2020-2022 rsyocto GmbH & Co. KG                                              |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
	}

	return 0;
}