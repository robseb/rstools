
/**
 *  
 * @file    main.cpp
 * @brief   FPGA-status
 * @author  rsyocto GmbH & Co. KG 
 * 			Robin Sebastian (git@robseb.de)
 * @mainpage
 * rstools application to dump a HSP-to-FPGA Bridges or the MPU address space
 * 
 * Change Log:  
 * 		1.00 (03-07-2022)
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
#include <bits/stdc++.h>

using namespace std;

// Application specific configurations
#define APP_MAX_ROW			300 


// Bridge Interfaces Base addresses 
#define LWHPSFPGA_OFST  	0xff200000 // LWHPS2FPGA Bridge 
#define HPSFPGA_OFST    	0xC0000000 // HPS2FPGA Bridge 
#define MPU_OFSET			0x0        // MPU (HPS Address space)

#define FPGAMAN_GPI_OFST    0xFF706014

// Bridge interface End address 
#define LWHPSFPGA_END   	0xFF3FFFFF
#define HPSFPGA_END     	0xFBFFFFFF
#define MPU_END         	0xFFFFFFFF
space
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

/*
*	@param  Add spaces to a string to achieve a specific length
*   @param  input 		String as input
*   @param  len			total length to achieve
*	@return string with the total length
*/
std::string fixStrlen(std::string input, uint8_t len)
{
	std::string output=input;
	for(uint8_t i=input.length(); i<len;i++)
	{
		output = output + ' ';
	}
	return output;
} 



int main(int argc, const char* argv[])
{
	// Error -> does not start at 0 
	argc -=1;
	/*
	argv[1] = "-lw";
	argv[2] =(const char*) "0"; // Address 
	argv[3] =":";
	argv[4] = (const char*)"100"; // Offset
	argc = 4;
	*/
	

	// Read a Register of the light Lightweight or AXI HPS to FPGA Interface
	if (( ((argc >3) && (std::string(argv[1]) == "-lw")) || ((argc > 3) && (std::string(argv[1]) == "-hf")) \
		|| ((argc > 3) && (std::string(argv[1]) == "-mpu"))) && (std::string(argv[3]) == ":"))
	{
		// Read the selected Bridge Interface 
		uint8_t address_space = 0; // 0: HPS2FPGA | 1: LWHPS2FPGA | 2: MPU
		bool lwBdrige = false;
		uint32_t addressStartOffset = 0;
		uint32_t addressEndOffset = 0;
		uint32_t address_start =0;
		uint32_t address_end =0;

		if (std::string(argv[1]) == "-lw") 
		{
			address_space = 1;
			lwBdrige = true;
		}
		else  if(std::string(argv[1]) == "-mpu")
		    address_space = 2;
		
		bool decMode = false;
		std::string ValueString;
		bool InputVailed = true;

		// Check if the decMode was enabled
		if ((argc > 4) && (std::string(argv[5]) == "-d"))
			decMode = true;

		/// Check the user inputs ///
		std::string AddresshexString = argv[2];
		std::string AddressEndStr	 = argv[4];

		// check if the address hex input is vailed
		if ((checkIfInputIsVailed(AddresshexString, false)) ||  (checkIfInputIsVailed(AddressEndStr, false)))
		{
			istringstream buffer(AddresshexString);
			buffer >> hex >> addressStartOffset;

			istringstream buffer2(AddressEndStr);
			buffer2 >> hex >> addressEndOffset;

			// Check for max Row 
			if(addressEndOffset > APP_MAX_ROW*16)
			{
				cout << "[ ERROR ]  Maximum number of rows "<<APP_MAX_ROW<<" reached !" << endl;
				cout << "           Maximum allowed range is: 0x"<<hex<<APP_MAX_ROW*16<<" reached !" <<dec<< endl;
				InputVailed = false;
			}

			// Start Address must be a 32-bit address
			if (addressStartOffset % 4 >0)
			{
				cout << "[ ERROR ]  The Start Address 0x"<<hex<<addressStartOffset<<" is not not a 32-bit Address" <<endl;
				cout << "           Use the next lower address: 0x"<<(addressStartOffset-(addressStartOffset%4))<<dec<<endl;
				InputVailed = false;
			}	
			// Start Address must be a 32-bit address
			if (addressEndOffset % 4 >0)
			{
				cout << "[ ERROR ]  The End Address Offset 0x"<<hex<<addressEndOffset<<" is not not a 32-bit Address" <<endl;
				cout << "           Use the next lower address: 0x"<<(addressEndOffset-(addressEndOffset%4))<<dec<<endl;
				InputVailed = false;
			}			
		

			// HPS2FPGA
			if (address_space == 0)
			{
				// check the range of the AXI HPS-to-FPGA Bridge Interface 
				if ((addressStartOffset+addressEndOffset) > H2F_RANGE)
				{

					cout << "[ ERROR ]  Selected Address is outside of the HPS to "\
					"FPGA AXI Bridge range!" << endl;
					InputVailed = false;
				}
			}
			// LWHPS2FPGA
			else if ((addressStartOffset+addressEndOffset)==1)
			{
				// check the range of the Lightweight HPS-to-FPGA Bridge Interface 
				if ((addressStartOffset+addressEndOffset) > LWH2F_RANGE)
				{

					cout << "[ ERROR ] Selected Address is outside of"\
					"the Lightweight HPS-to-FPGA Bridge Range!" << endl;
					InputVailed = false;
				}
			}
			// MPU
			else
			{
				// check the range of the MPU address space
				if ((addressStartOffset+addressEndOffset) > MPU_RANGE)
				{
					cout << "[  ERROR  ] Selected address is outside of"\
					"the HPS Address Range!" << endl;
					InputVailed = false;
				}
			}
		}
		else
		{
			// address input is not vadid

			cout << "[  ERROR  ] Selected Value Input is not HEX Address!" << endl;
			InputVailed = false;
		}

		if (address_space < 2)
			address_start = (lwBdrige ? LWHPSFPGA_OFST : HPSFPGA_OFST) + addressStartOffset;
		else
			address_start = addressStartOffset+addressEndOffset;
		
		address_end  = address_start +addressEndOffset;

		// only in case the input is valid read the bridge
		if (InputVailed)
		{
			cout << "-----------------------------------------MEMORY DUMP --------------------------------------------------" << endl;
			if (address_space < 2)
			{
				cout << "	Bridge:      " << (lwBdrige ? "Lightweight HPS-to-FPGA" : "HPS-to-FPGA");
				cout << "	   Brige Base:  0x" << hex << (lwBdrige ? LWHPSFPGA_OFST : HPSFPGA_OFST) << dec << endl;
			}
			else
			{
				cout << "  MPU Address Range"<<endl;
			}

			cout << "	Your Start Offset: 0x" << hex << addressStartOffset << dec << endl;
			cout << "	Your End Offset: 0x" << hex << addressEndOffset << dec << endl;
			cout << "	Range Address:     0x" << hex << address_start <<" : "<<address_end<< dec << endl;
	
			cout << "   Encoding:      uint32_t [High - Low] in "<<(decMode ? "DEC": "HEX" )<< endl;
		
			do
			{
				void* bridgeMap;
				int fd;

				// open memory driver 
				fd = open("/dev/mem", (O_RDWR | O_SYNC));

				// was opening okay
				if (fd < 0)
				{
					cout << "[ ERROR ] Failed to open memory driver!" << endl;
				}

				bridgeMap = mmap(NULL, addressEndOffset, PROT_READ, MAP_PRIVATE, fd, \
						address_start & ~MAP_MASK);

				// check if opening was successfully
				if (bridgeMap == MAP_FAILED)
				{
					cout << "[ ERROR ]  Accessing the virtual memory failed!" << endl;
					close(fd);
					break;
				}
				void* readMap = bridgeMap + (address_start & MAP_MASK);
				uint32_t address_curent =0;
				
				if (!decMode)
				{
					// For the HEX Format Output Mode
					cout << "-------------------------------------------------------------------------------------------------------" << endl;
					cout << "| Offset  |   Address   || O-H   0-L  |  1-H   1-L  |  2-H   2-L  |  3-H   3-L     || ASCII"<<endl;
					cout << "-------------------------------------------------------------------------------------------------------" << endl;
				}
				else
				{
					// For the DEC Format Output Mode
					cout << "-------------------------------------------------------------------------------------------------------" << endl;
					cout << "| Offset  |   Address   ||      O     |      1     |      2     |      3         || ASCII"<<endl;
					cout << "-------------------------------------------------------------------------------------------------------" << endl;
				}
				

				// Write row after row 
				for (uint16_t row=0; row<=addressEndOffset; row+=16)
				{
					// calucate the addresss for the row
					address_curent = address_start+row;
					
					// At the beginning of the row print  the address
					std::stringstream stream;
					stream << std::hex << row;    
					cout << "| 0x"<<fixStrlen(stream.str(),6);
					stream.str(std::string());
					stream << std::hex << address_curent;
					cout << "| 0x"<<fixStrlen(stream.str(),10)<<"||";
					std::string ascii ="";

					// Write to each Column 4 32-bit values 
					for (uint16_t i = 0; i < 16; i+=4)
					{
						// read the 32-Bit Value 
						readMap = bridgeMap + ((address_curent+i) & MAP_MASK);
						uint32_t value = *((uint32_t*)readMap);
						// value= (uint32_t) 0x416231; // ASCII = "Ab1"
						// read the high byte
						uint16_t hi	= (value >> 16);
						uint16_t lo	= (value & 0xFFFF);
						
						if(!decMode)
						{
							stream.str(std::string());
							stream << std::hex << hi;
							cout << " "<<fixStrlen(stream.str(),4);

							// read the low byte
							stream.str(std::string());
							stream << std::hex << lo;
							if (i<12)
								cout<<"  "<<fixStrlen(stream.str(),4) <<" | ";
							else
								cout<<"  "<<fixStrlen(stream.str(),4) <<"    ";
						}
						else
						{	
							// For DEC Output Format Mode
							if (i<12)
								cout<<" "<<fixStrlen(to_string(value),10) <<" |";
							else 
								cout<<" "<<fixStrlen(to_string(value),10) <<"     ";
						}
					
						
						// Convert row to 16 ASCII Chars 
						
						ascii += stoul(to_string ((hi & 0xFF00)>>8), nullptr, 10);
						ascii += stoul(to_string (hi & 0x00FF), nullptr, 10);
						ascii += stoul(to_string ((lo & 0xFF00)>>8), nullptr, 10);
						ascii += stoul(to_string (lo & 0x00FF), nullptr, 10);
						
					}
					cout << "|| ";
					// Remove New Line feeds and unkwon ASCII of ASCII String  
					for (int i = 0; i < ascii.length();i++) 
					{
						if (((uint8_t) ascii[i] <32) || ((uint8_t) ascii[i] >126))
						{
							ascii[i] = ' ';
						}
					}				
					cout <<ascii<< endl;
				}
				if (addressEndOffset>100)
				{																							
					if (!decMode)
					{
						// For the HEX Format Output Mode
						cout << "-------------------------------------------------------------------------------------------------------" << endl;
						cout << "| Offset  |   Address   || O-H   0-L  |  1-H   1-L  |  2-H   2-L  |  3-H   3-L     || ASCII"<<endl;
						cout << "-------------------------------------------------------------------------------------------------------" << endl;
					}
					else
					{
						// For the DEC Format Output Mode
						cout << "-------------------------------------------------------------------------------------------------------" << endl;
						cout << "| Offset  |   Address   ||      O     |      1     |      2     |      3         || ASCII"<<endl;
						cout << "-------------------------------------------------------------------------------------------------------" << endl;
					}
				}
				// Close the MAP 
				if (munmap(bridgeMap, addressEndOffset) < 0)
				{
					cout << "[ ERROR ] Closing of shared memory failed!" << endl;
				}

				// Close the driver port 
				close(fd);

			} while (0);
		}
		else
		{
			cout << "[ ERROR ] User Input is wrong!"<<endl;
			cout <<	"          FPGA-dumpBridge -lw|hf|mpu <Address Offset in HEX> : <Offset to Dump in HEX>  -d"<< endl;
			
		}
	}
	else
	{
		// help output 
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "|        Command to dump a Memory Area of an HPS-to-FPGA Bridge Interface                    |" << endl;
		cout << "|                    or of the entire MPU (HPS) Memory Space                                 |" << endl;
		cout << "|                         Designed for Intel SoC FPGAs                                       |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "|$ FPGA-dumpBridge -lw [Address Offset in HEX] : [Offset to Dump in HEX]                     |" << endl;
		cout << "|      L   Reading of a 32-bit Lightweight HPS-to-FPGA Bridge Register                       |" << endl;
		cout << "|          e.g.: FPGA-dumpBridge -lw 0A : 10                                                 |" << endl;
		cout << "|$ FPGA-dumpBridge -hf [Address Offset in HEX] : [Offset to Dump in HEX]                     |" << endl;
		cout << "|      L   Reading of a 32-bit of the HPS-to-FPGA AXI Bridge Register                        |" << endl;
		cout << "|          e.g.: FPGA-dumpBridge -hf 8C : FF                                                 |" << endl;
		cout << "|$ FPGA-dumpBridge -mpu [Address Offset in HEX] : [Offset to Dump in HEX]                    |" << endl;
		cout << "|      L   Reading of a 32-bit Register of the entire MPU (HPS) memory Space                 |" << endl;
		cout << "|          e.g.: FPGA-dumpBridge -mpu 87 : FF                                                |" << endl;
		cout << "|                                                                                            |" << endl;
		cout << "|      Suffix: -d -> Dump as uint32_t DEC                                                    |" << endl;
		cout << "|$ FPGA-dumpBridge -lw|hf|mpu <Address Offset in HEX> : <Offset to Dump in HEX>  -d          |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
		cout << "| Vers.: "<<VERSION<<"                                                                                |"<<endl;
		cout << "| Copyright (C) 2021-2022 rsyocto GmbH & Co. KG                                              |" << endl;
		cout << "----------------------------------------------------------------------------------------------" << endl;
	}

	return 0;
}
