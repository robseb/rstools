/**
 *  
 * @file    main.cpp
 * @brief   FPGA-status
 * @author  rsyocto GmbH & Co. KG 
 * 			Robin Sebastian (git@robseb.de)
 * @mainpage
 * rstools application to write a new configuration to 
 * 
 * Change Log:  
 * 		1.00 (03-08-2022)
 * 		Initial release
 * 
 * Copyright (C) 2020-2022 rsyocto GmbH & Co. KG  *  All Rights Reserved
 * 
 */

#define VERSION "1.00"

extern "C"
{
volatile void* __hps_virtualAdreess_FPGAMGR;
volatile void* __hps_virtualAdreess_FPGAMFRDATA;
volatile int __fd;
}

#include <cstdio>
#include "alt_fpga_manager.h"
#include "hps.h"
#include <string.h>
#include <fstream>
#include <iostream>
#include <thread>					// Required for putting task to sleep 
#include <chrono>					// Required for putting task to sleep 
using namespace std;


bool is_file_exist(const char* fileName)
{
	ifstream infile(fileName);
	return infile.good();
}



/*
*   @brief               	Perform HPS to FPGA Reset
*	@param	ConsloeOutput	Print Status Output to Console  
* 	@param	reset_typ		1 = FPGA Warm Reset
*							2 = FPGA Cold Reset
*							3 = LW HPS-to-FPGA Bridge Reset
*							4 = HPS-to-FPGA Bridge Reset
*							5 = FPGA-to-HPS Bridge Reset
*   @return                 success  
*/ 
bool performHPStoFPGAReset(bool ConsloeOutput, uint8_t reset_typ)
{
	// Print the Inteted Reset Operation
	if (ConsloeOutput)
	{
		switch(reset_typ)
		{
			case 1: cout <<"#    Performing HPS-to-FPGA Warm Reset  (h2f_rst_n = 1,0)"<<endl; break;
			case 2: cout <<"#    Performing HPS-to-FPGA Cold Reset  (h2f_cold_rst_n = 1,0)"<<endl; break;

			case 3: cout <<"#    Performing a reset on the LightWeight HPS-to-FPGA Bridge"<<endl; break;
			case 4: cout <<"#    Performing a reset on the HPS-to-FPGA Bridge"<<endl; break;
			case 5: cout <<"#    Performing a reset on the FPGA-to-HPS Bridge"<<endl; break;
			default:
				cout <<"[ERROR]  Unkown Reset Type to perform!"<<endl; break;
				return false;
		}
	}

	// RESET =1 

	// Perform Cold or Warm FPGA Reset
	if(reset_typ ==1) 		(void)  ((int) system( (const char*) "FPGA-writeBridge -mpu 0xFFD05020 -b 6 1 -b"));
	else if(reset_typ ==2)  (void)  ((int) system( (const char*) "FPGA-writeBridge -mpu 0xFFD05020 -b 7 1 -b"));
	// Perform a Bridge Reset
	else if(reset_typ==3)	(void) ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD0501C -b 1 1 -b"));
	else if(reset_typ==4)	(void) ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD0501C -b 0 1 -b"));
	else if(reset_typ==5)	(void)  ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD0501C -b 2 1 -b"));

	// Wait 50ms
	// C++11: Put this task to sleep 
	std::this_thread::sleep_until(std::chrono::system_clock::now() + \
		std::chrono::milliseconds(50));

	// RESET =0

	// Perform Cold or Warm FPGA Reset
	if(reset_typ ==1) 	   (void)  ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD05020 -b 6 0 -b"));
	else if(reset_typ ==2) (void)  ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD05020 -b 7 0 -b"));
	// Perform a Bridge Reset
	else if(reset_typ==3)	(void)  ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD0501C -b 1 0 -b"));
	else if(reset_typ==4)	(void)  ((int) system((const char*) "FPGA-writeBridge -mpu 0xFFD0501C -b 0 0 -b"));
	else if(reset_typ==5)	(void)  ((int) system( (const char*) "FPGA-writeBridge -mpu 0xFFD0501C -b 2 0 -b"));

	if(ConsloeOutput)
		cout << "[SUCCESS] Reset performed"<<endl;
	else
		cout << "1";

	return true;
}

bool writeFPGAconfig(const char* configFileAdress, bool withOutput)
{
	/////////ceck vailed FPGA status  /////////

	/// check if the input file exist  
	if (!is_file_exist(configFileAdress))
	{
		if (withOutput)
			cout << "[ ERROR ] The selected config file does not exsist!" << endl;
		return false;
	}

	/// Load the FPGA configuration file
	if (withOutput)
		cout << "[ INFO ] Start writing the new FPGA configuration" << endl;

	// Open rbf config and load them to an binary buffer into the Memory
	FILE* f = fopen(configFileAdress, "rb");
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = new char[fsize + 1];
	fread(buf, 1, fsize, f);
	fclose(f);

	// Start to write the FPGA Configuration
	ALT_STATUS_CODE status = alt_fpga_configure(buf, fsize);

	if (status != ALT_E_SUCCESS)
	{
		if (withOutput)
			cout << "[ ERROR ] Writing the FPGA configuration failed" << endl;
		return false;
	}
	else
	{
		if (withOutput)
			cout << "[ SUCCESS ] The FPGA runs now with the new configuration" << endl;

		// Reset all Bridges
		if (withOutput)
			cout << "[ INFO] Performing a reset on all Bridge Interfaces" <<endl;
		
		for (uint8_t i=3; i<6;i++)
		{
			performHPStoFPGAReset(withOutput,i);
		}

		// Perform COLD FPGA Reset
		performHPStoFPGAReset(withOutput,2);
		return true;
	}

	return false;
}



int main(int argc, const char* argv[])
{

	///////// init the Virtual Memory for I/O access /////////
	__VIRTUALMEM_SPACE_INIT();

	/////////	 init the FPGA Manager	 /////////
	alt_fpga_init();

	///////// Take the right to controll the FPGA /////////
	alt_fpga_control_enable();

	ALT_FPGA_STATE_t stat = alt_fpga_state_get();

	// change to a new selected FPGA configuration
	if ((argc > 2) && (std::string(argv[1]) == "-f"))
	{
		bool withOutput = !((argc > 3) && (std::string(argv[3]) == "-b"));
		bool res = writeFPGAconfig(argv[2], withOutput);

		if (!withOutput) cout << res ? 1 : 0;
	}
	// restore the the boot up configuration 
	else if ((argc > 1) && (std::string(argv[1]) == "-r"))
	{
		bool withOutput = !((argc > 2) && (std::string(argv[2]) == "-b"));
		bool res = writeFPGAconfig("/usr/rsyocto/running_bootloader_fpgaconfig.rbf", withOutput);
		if (!withOutput) cout << res ? 1 : 0;
	}
	else 
	{
		cout << "	Command to change the FPGA fabric configuration" << endl;
		cout << "	FPGA-writeConfig -f [config rbf file path] {-b [optional]}" << endl;
		cout << "		change the FPGA config with a selected .rbf file" << endl;
		cout << "	FPGA-writeConfig -r {-b [optional]}" << endl;
		cout << "		restore to the boot up FPGA configuration" << endl;
		cout << "		this conf File is located: /usr/rsyocto/running_bootloader_fpgaconfig.rbf" << endl;
		cout << "		suffix: -b -> only decimal result output"<<endl;
		cout << "						Error:  0" << endl;
		cout << "						Succses:1" << endl;
		cout <<endl <<"Vers.: "<<VERSION<<endl;
		cout <<"Copyright (C) 2020-2022 rsyocto GmbH & Co. KG" << endl;

	}


	// Give the right to controll the FPGA
	alt_fpga_control_disable();

	// free the dynamic access memory
	__VIRTUALMEM_SPACE_DEINIT();

	return 0;
}
