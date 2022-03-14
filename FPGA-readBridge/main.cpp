
/**
 *  
 * @file    main.cpp
 * @brief   FPGA-status
 * @author  rsyocto GmbH & Co. KG 
 * 			Robin Sebastian (git@robseb.de)
 * @mainpage
 * rstools application to read any HSP-to-FPGA Bridges or the MPU address space
 * 
 * Change Log:  
 * 		1.00 (12-07-2019)
 * 		Initial release
 * 
 * Copyright (C) 2020-2022 rsyocto GmbH & Co. KG  *  All Rights Reserved
 * 
 */

#define VERSION "1.00"

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

using namespace std;

// Bridge Interfaces Base addresses 
#define LWHPSFPGA_OFST  	0xff200000 // LWHPS2FPGA Bridge 
#define HPSFPGA_OFST    	0xC0000000 // HPS2FPGA Bridge 
#define MPU_OFSET			0x0        // MPU (HPS Address space)

#define FPGAMAN_GPI_OFST    0xFF706014

// Bridge interface End address 
#define LWHPSFPGA_END   	0xFF3FFFFF
#define HPSFPGA_END     	0xFBFFFFFF
#define MPU_END         	0xFFFFFFFF

// Bridge interface range (allowed input offset)
#define LWH2F_RANGE    (LWHPSFPGA_END - LWHPSFPGA_OFST)
#define H2F_RANGE      (HPSFPGA_END - HPSFPGA_OFST)
#define MPU_RANGE      (MPU_END - MPU_OFSET)

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// Auto refresh Mode settings
#define REFRECHMODE_DELAY_MS	50
#define REFRECHMODE_DURATION_MS 15000
#define REFRECHMODE_MAX_COUNT   (REFRECHMODE_DURATION_MS/REFRECHMODE_DELAY_MS)

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
	if ((input.find_first_of("0x",0) == 0) && (!DecHex))
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
	// Read a Register of the light Lightweight or AXI HPS to FPGA Interface
	if (((argc >2) && (std::string(argv[1]) == "-lw")) || ((argc > 2) && (std::string(argv[1]) == "-hf")) \
		|| ((argc > 2) && (std::string(argv[1]) == "-mpu")) || ((argc > 1) && (std::string(argv[1]) == "-gpi")))
	{
		// Read the selected Bridge Interface 
		uint8_t address_space = 0; // 0: HPS2FPGA | 1: LWHPS2FPGA | 2: MPU
		bool lwBdrige = false;
		bool gpi_read_mode = false;
		uint32_t addressOffset = 0;
		uint32_t address =0;
		uint8_t arg_no=0;

		if (std::string(argv[1]) == "-lw") 
		{
			address_space = 1;
			lwBdrige = true;
		}
		else  if(std::string(argv[1]) == "-mpu")
		    address_space = 2;
		
		else  if(std::string(argv[1]) == "-gpi")
		{	
			// Enable reading GPI (FPGA->HPS) Register 
			// Using MPU mode with fixed address 
			gpi_read_mode = true;
			address_space = 2;
			address = FPGAMAN_GPI_OFST;
			arg_no =1;
		}

		bool ConsloeOutput = true;
		bool refreshMode = false;
		std::string ValueString;
		bool InputVailed = true;

		// check if only decimal output is enabled 
		if ((argc > (3-arg_no)) && (std::string(argv[3-arg_no]) == "-b"))
			ConsloeOutput = false;

		// Check if the refreshMode was enabled
		if ((argc > (3-arg_no)) && (std::string(argv[3-arg_no]) == "-r"))
			refreshMode = true;

		// For GPI reading do not process input address offset
		if(!gpi_read_mode)
		{
			/// Check the user inputs ///
			std::string AddresshexString = argv[2];
	
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
							cout << "	ERROR: selected address is outside of the HPS to "\
							"FPGA AXI Bridge range!" << endl;
						InputVailed = false;
					}
				}
				// LWHPS2FPGA
				else if (address_space==1)
				{
					// check the range of the Lightweight HPS-to-FPGA Bridge Interface 
					if (addressOffset > LWH2F_RANGE)
					{
						if (ConsloeOutput)
							cout << "	ERROR: selected address is outside of"\
							"the Lightweight HPS-to-FPGA Bridge range!" << endl;
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
				// address input is not vadid
				if (ConsloeOutput)
					cout << "[  ERROR  ] Selected Value Input is a HEX Address!" << endl;
				InputVailed = false;
			}

			if (address_space < 2)
				address = (lwBdrige ? LWHPSFPGA_OFST : HPSFPGA_OFST) + addressOffset;
			else
				address = addressOffset;

		}
		
		// only in case the input is valid read the bridge
		if (InputVailed)
		{
			if (ConsloeOutput)
			{	
				cout << "------------------------------------READING------------------------------------------" << endl;
				if (address_space < 2)
				{
					cout << "   Bridge:      " << (lwBdrige ? "Lightweight HPS-to-FPGA" : "HPS-to-FPGA");
					cout << "      Brige Base:  0x" << hex << (lwBdrige ? LWHPSFPGA_OFST : HPSFPGA_OFST) << dec << endl;
					cout << "   Your Offset: 0x" << hex << addressOffset << dec;
					cout << "   Address:     0x" << hex << address << dec << endl;
				}
				else 
				{	
					if (!gpi_read_mode)
					{
						cout << "   Brige Base:  0x00 (MPU Address Space)"<< endl;
						cout << "   Address:     0x" << hex << address << dec << endl;
					}
					else
					{
						cout << "   Brige Base: 32-bit GPI (General-Purpose Input Register) FPGA->HPS " << endl;
						cout << "   Address:     0x" << hex << FPGAMAN_GPI_OFST << dec << endl;
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

				bridgeMap = mmap(NULL, 4, PROT_READ, MAP_PRIVATE, fd, \
						address & ~MAP_MASK);
				// check if opening was successfully
				if (bridgeMap == MAP_FAILED)
				{
					if (ConsloeOutput)
						cout << "ERROR: Accessing the virtual memory failed!" << endl;
					else
						cout << -2;
					close(fd);
					break;
				}
				void* readMap = bridgeMap + (address & MAP_MASK);
				uint16_t delay_count = 0;
				do
				{
					// Read the address 
					uint32_t value = *((uint32_t*)readMap);

					if (ConsloeOutput)
					{
						cout << "-------------------------------------------------------------------------------------" << endl;
						cout << "			      Value: " << value << " [0x" << hex << value << "]" << dec << endl;
						cout << "-------------------------------------------------------------------------------------" << endl;
						cout << "No  |";
						for (uint16_t i = 31; i > 15; i--)
						{
							cout << " " << i << " |";
						}
						cout << endl << "Bit |";
						for (uint16_t i = 31; i > 15; i--)
						{
							cout << "  " << (value & (1 << i) ? 1 : 0) << " |";
						}
						cout << endl;
						cout << "-------------------------------------------------------------------------------------" << endl;

						cout << "No  |";
						for (int16_t i = 15; i >= 0; i--)
						{
							if (i > 9)
								cout << " " << i << " |";
							else
								cout << " 0" << i << " |";
						}
						cout << endl << "Bit |";
						for (int16_t i = 15; i >= 0; i--)
						{
							cout << "  " << (value & (1 << i) ? 1 : 0) << " |";
						}
						cout << endl;
						cout << "-------------------------------------------------------------------------------------" << endl;
					}
					else
					{
						// output only the value as decimal 
						cout << value;
					}

					if (!refreshMode)
						break;
					else
					{
						delay_count++;
						// Print the refrech status
						cout << "Auto Refrech Mode for " << REFRECHMODE_DURATION_MS << "ms [" << delay_count << \
							"/" << REFRECHMODE_MAX_COUNT << "]" << endl;

						// C++11: Put this task to sleep 
						std::this_thread::sleep_until(std::chrono::system_clock::now() + \
							std::chrono::milliseconds(REFRECHMODE_DELAY_MS));
						// Remove the last 10 rows 
						if (delay_count < REFRECHMODE_MAX_COUNT)
							cout << "\033[F\033[F\033[F\033[F\033[F\033[F\033[F\033[F\033[F\033[F";
					}

				} while (delay_count<REFRECHMODE_MAX_COUNT);

				// Close the MAP 
				if (munmap(bridgeMap, 4) < 0)
				{
					if (ConsloeOutput)
						cout << "[ ERROR ] Closing of shared memory failed!" << endl;
				}

				// Close the driver port 
				close(fd);


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
				cout <<	"          FPGA-readBridge -lw|hf|mpu|gpi <Address Offset in HEX> -b|r"<< endl;
			}
		}
	}
	else
	{
		// help output 
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "|        Command to read a 32-bit register of a HPS-to-FPGA Bridge Interface                 |" << endl;			
		cout << "|                    or of the entire MPU (HPS) Memory space                                 |" << endl;
		cout << "|                         Designed for Intel SoC FPGAs                                       |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "|$ FPGA-readBridge -lw [Address Offset in HEX]                                               |" << endl;
		cout << "|      L   Reading of a 32-bit Lightweight HPS-to-FPGA Bridge Register                       |" << endl;
		cout << "|          e.g.: FPGA-readBridge -lw 0A                                                      |" << endl;
		cout << "|$ FPGA-readBridge -hf [Address Offset in HEX]                                               |" << endl;
		cout << "|      L   Reading of a 32-bit of the HPS-to-FPGA AXI Bridge Register                        |" << endl;
		cout << "|          e.g.: FPGA-readBridge -hf 8C                                                      |" << endl;
		cout << "|$ FPGA-readBridge -gpi                                                                      |" << endl;
		cout << "|      L   Reading of the 32-bit GPI (General-Purpose Input Register) FPGA->HPS Register     |" << endl;
		cout << "|          e.g.: FPGA-readBridge -gpi                                                        |" << endl;
		cout << "|$ FPGA-readBridge -mpu [Address Offset in HEX]                                              |" << endl;
		cout << "|      L   Reading of a 32-bit Register of the entire MPU (HPS) memory space                 |" << endl;
		cout << "|          e.g.: FPGA-readBridge -mpu 87                                                     |" << endl;
		cout << "|                                                                                            |" << endl;
		cout << "|      Suffix: -b -> only decimal result output                                              |" << endl;
		cout << "|                     L -1 = Input Error                                                     |" << endl;
		cout << "|                     L -2 = Linux Kernel Memory Error                                       |" << endl;
		cout << "|      Suffix: -r -> Auto refrech the value for 15sec                                        |" << endl;
		cout << "|$ FPGA-readBridge -lw|hf|mpu|gpi <Address Offset in HEX> -b|r                               |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "| Vers.: "<<VERSION<<"                                                                                |"<<endl;
		cout << "| Copyright (C) 2021-2022 rsyocto GmbH & Co. KG                                              |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
	}

	return 0;
}
