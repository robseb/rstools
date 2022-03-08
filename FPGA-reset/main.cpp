/**
 *  
 * @file    main.cpp
 * @brief   FPGA-status
 * @author  rsyocto GmbH & Co. KG 
 * 			Robin Sebastian (git@robseb.de)
 * @mainpage
 * rstools application to reset HPS <> FPGA Interfaces and the FPGA Fabric 
 * 
 * Change Log:  
 * 		1.00 (03-08-2022)
 * 		Initial release
 * 
 * Copyright (C) 2020-2022 rsyocto GmbH & Co. KG  *  All Rights Reserved
 * 
 */

#define VERSION "1.00"


#include <iostream>
#include <fstream>					// POSIX: for acessing Linux drivers
#include <sys/mman.h>				// POSIX: memory maping
#include <fcntl.h>					// POSIX: "PROT_WRITE", "MAP_SHARED", ...
#include <unistd.h>					// POSIX: for closing the Linux driver access
#include <cstdint>                  // Standard integral types (uint8_t,...)
#include <thread>					// Required for putting task to sleep 
#include <chrono>					// Required for putting task to sleep 


using namespace std;

/*
*
* Registers
*/

// FPGA Manager Status Register (MSEL, Status)
#define REG_FPGAMG_STATUS			0xFF706000
#define REG_FPGAMG_STATUS_OFFSET	0x0
#define REG_FPGAMG_CTL				0xFF706004
#define REG_FPGAMG_CTL_OFFSET		0x4

#define REG_FPGAMG_CTL_EN			(1<<0)
#define REG_FPGAMG_CTL_nCONFIG		(1<<2)
/*
* Global Values
*/

volatile int posix_fd;
volatile void* fpgaMangerMap;    	// Memory Map assigned to FPGA Manger
volatile uint32_t* ptrFpgaManger;   // Pointer to FPGA Manager Registers 

/*
*   @brief               Convert FPGA State Code to String with state description 
*   @param	state_code	 FPGA Fabric Sate Code (acording datasheet)
*   @return              description string
*/ 
string state2str(uint8_t state_code)
{
	switch (state_code)
	{
	case 0x00:
		return "		0x00 FPGA Powered Off";
	case 0x01:
		return "		0x01 FPGA in Reset Phase";
	case 0x02:
		return "		0x02 FPGA in Configuration Phase";
	case 0x03:
		return "		0x03 FPGA in Initialization Phase.\n" \
			   "			 In CVP configuration, this state indicates IO configuration has completed.";
	case 0x04:
		return "		0x04 FPGA in User Mode";
	case 0x05:
		return "		0x05 FPGA state has not yet been determined.\n" \
			   "			 This only occurs briefly after reset.";
	default:
		return "		     ERROR FPGA FABRIC PHASE IS UNKNWON!";
	}
	return "";
}

/*
*   @brief              Read FPGA Fabric State 
*   @return             State Code         
*/ 
uint8_t readState(void)
{	
	if (ptrFpgaManger==0) return 0xFF;
	// Read Bit 0-3 of the FPGA Manager Status register
	uint32_t stat = *(ptrFpgaManger + (REG_FPGAMG_STATUS_OFFSET)) & 0x7;
	return (uint8_t) stat;
}


/*
*   @brief               	 Perform a Reset of the FPGA Fabric 
*						  	(deletes running content and brings Fabric in Reset State)
*	@param	ConsloeOutput	Print Status Output to Console  	
*   @return                 success  
*/ 
bool performFPGAfabricClear(bool ConsloeOutput)
{
	if (ConsloeOutput)
		cout <<"#    Performing FPGA Fabric Reset"<<endl;

	// Enable HPS access to FPGA Manager mode
	system("FPGA-writeBridge -mpu 0xFF706004 -b 0 1 -b");
	
	// Reset the FPGA Fabric by Setting nCONFIG = High
	if(ConsloeOutput)
        cout << "[INFO] Pull-down nCONFIG input to the CB. This puts the FPGA in reset phase and restarts configuration."<<endl;

	// Enable HPS access to FPGA Manager mode
	system("FPGA-writeBridge -mpu 0xFF706004 -b 2 1 -b");

	// Leave the HPS access to FPGA Manager mode
	system("FPGA-writeBridge -mpu 0xFF706004 -b 0 0 -b");

	// Check that the FPGA Fabric is in Reset State
	if(!readState()==0x01)
	{
		if(ConsloeOutput)
			cout << "\n[ERORR] After the FPGA Fabric Reset, the FPGA is not in the Reset State"<<endl;
		else
			cout << "-1";
		return false;
	}	
	if(ConsloeOutput)
		cout << "[SUCCESS] FPGA Fabric is cleared and is in Reset State"<<endl;
	else
		cout << "1";

	return true;
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

	// Peform Cold or Warm FPGA Reset
	if(reset_typ ==1) 		system("FPGA-writeBridge -mpu 0xFFD05020 -b 6 1 -b");
	else if(reset_typ ==2) system("FPGA-writeBridge -mpu 0xFFD05020 -b 7 1 -b");
	// Peform a Bridge Reset
	else if(reset_typ==3)	system("FPGA-writeBridge -mpu 0xFFD0501C -b 1 1 -b");
	else if(reset_typ==4)	system("FPGA-writeBridge -mpu 0xFFD0501C -b 0 1 -b");
	else if(reset_typ==5)	system("FPGA-writeBridge -mpu 0xFFD0501C -b 2 1 -b");

	// Wait 50ms
	// C++11: Put this task to sleep 
	std::this_thread::sleep_until(std::chrono::system_clock::now() + \
		std::chrono::milliseconds(50));

	// RESET =0

	// Peform Cold or Warm FPGA Reset
	if(reset_typ ==1) 		system("FPGA-writeBridge -mpu 0xFFD05020 -b 6 0 -b");
	else if(reset_typ ==2) system("FPGA-writeBridge -mpu 0xFFD05020 -b 7 0 -b");
	// Peform a Bridge Reset
	else if(reset_typ==3)	system("FPGA-writeBridge -mpu 0xFFD0501C -b 1 0 -b");
	else if(reset_typ==4)	system("FPGA-writeBridge -mpu 0xFFD0501C -b 0 0 -b");
	else if(reset_typ==5)	system("FPGA-writeBridge -mpu 0xFFD0501C -b 2 0 -b");

	if(ConsloeOutput)
		cout << "[SUCCESS] Reset performed"<<endl;
	else
		cout << "1";

	return true;
}



/*
*   @brief                Inititalisation of HPS Register Maps 
*						  FPGA Manager Status Register: 0xFF706000 - 0xFF706004
*   @return               success  
*/ 
int initMemRegs(void)
{
	// Open Linux Memory Driver port
	posix_fd = open("/dev/mem", (O_RDWR | O_SYNC));

    // Check that the opening was sucsessfull 
	if (posix_fd < 0)
	{
		cout << "[ERROR]  Failed to read the memory driver!" << endl;
		return -1;
	}

	//
	// FPGA Manager Address Space
	//

    // Create a Memory Map to acess the FPGA Manger
	fpgaMangerMap = mmap(NULL, 4, PROT_READ, MAP_PRIVATE, posix_fd, \
		REG_FPGAMG_STATUS);

    if (fpgaMangerMap == MAP_FAILED)
	{
		cout << "\n[ERROR] Failed to open the memory maped interface to the FPGA Manger" << endl;
        return -1;
	}

    // Allocate a pointer to the maped address space 
    ptrFpgaManger = (uint32_t*)fpgaMangerMap;

    if (ptrFpgaManger==0) 
    {
        cout << "\n[ERROR] Failed to allocate a ponter to the FPGA Manager Address Space"<<endl;
        return -1;
    }

    return 1;
}

/* 
 * @ brief      Deinit all open MAPs and ports  
 *
*/
void deinit(void)
{
    // Close the MMAP for the FPGA Manager Address Space
    if (fpgaMangerMap != MAP_FAILED)
    {
        munmap((void*) fpgaMangerMap, 4);
    }

    // close POSIX port
    close(posix_fd);
}





int main(int argc, const char* argv[])
{
	if(initMemRegs()==-1) return -1;
	uint8_t  state_code = readState();

	/*
	argv[1] = "-ffc";
	argc =1;
	*/

	if (argc > 1)
	{
		if (std::string(argv[1]) == "-h")
		{
			cout << "	Command to read and perform the HPS to FPGA resets" << endl;
			cout << "   Warm/Cold FPGA Reset, HPS<>FPGA Brdige Resets, FPGA Fabric Reset" <<endl;
			cout << "	FPGA-reset -fwr|fcr|lwr|hfr|fhr|ffc -d" << endl;
			cout << "       -fwr        => HPS to FPGA Warm Reset (h2f_rst_n = 1,0)"<<endl;
			cout << "       -fcr        => HPS to FPGA Cold Reset (h2f_cold_rst_n =1,0)"<<endl;
			cout << "       -lwr        => Performs a reset on the LightWeight HPS-to-FPGA Bridge"<<endl;
			cout << "       -hfr        => Performs a reset on the HPS-to-FPGA Bridge"<<endl;
			cout << "       -fhr        => Performs a reset on the FPGA-to-HPS Bridge"<<endl;
			cout << "       -ffc        => FPGA Fabric Reset (deletes running content and brings Fabric in Reset State)"<<endl;
			cout << " Reset Periode: 50ms"<<endl;
			cout <<endl <<"Vers.: "<<VERSION<<endl;
			cout <<"Copyright (C) 2020-2022 rsyocto GmbH & Co. KG" << endl;

		}

		// Read Reset command and execute it
		uint8_t reset_type =0;
		bool ffc = false;
		bool fwr = false;
		bool fcr = false;
		bool lwr = false;
		bool hfr = false;
		bool fhr = false;

		bool ConsloeOutput = true;

		for (uint8_t i=1; i<argc;i++)
		{
			if		(std::string(argv[i])=="-fwr") fwr=true;
			else if (std::string(argv[i])=="-fcr") fcr=true;
			else if (std::string(argv[i])=="-lwr") lwr=true;
			else if (std::string(argv[i])=="-hfr") hfr=true;
			else if (std::string(argv[i])=="-fhr") fhr=true;

			else if (std::string(argv[i])=="-ffc") ffc = true;
			else if (std::string(argv[i])=="-b") ConsloeOutput = false;
		}

		if(ffc==true) 		performFPGAfabricClear(ConsloeOutput);
		if(fwr)				performHPStoFPGAReset(ConsloeOutput,1);
		if(fcr)				performHPStoFPGAReset(ConsloeOutput,2);
		if(lwr)				performHPStoFPGAReset(ConsloeOutput,3);
		if(hfr)				performHPStoFPGAReset(ConsloeOutput,4);
		if(fhr)				performHPStoFPGAReset(ConsloeOutput,5);
	}
	else
	{
		// Print the MSEL Value as detailed string 
		cout << "-- Perform HPS-to-FPGA and FPGA Resets --" << endl;

		cout << "# FPGA Fabric Operation State:" <<endl <<endl;
		cout << state2str(state_code) << endl;	

		cout << "# Perform Reset Options" <<endl;
		cout << "   -fwr        => HPS to FPGA Warm Reset (h2f_rst_n = 1,0)"<<endl;
		cout << "   -fcr        => HPS to FPGA Cold Reset (h2f_cold_rst_n =1,0)"<<endl;
		cout << "   -lwr        => Performs a reset on the LightWeight HPS-to-FPGA Bridge"<<endl;
		cout << "   -hfr        => Performs a reset on the HPS-to-FPGA Bridge"<<endl;
		cout << "   -fhr        => Performs a reset on the FPGA-to-HPS Bridge"<<endl;
		cout << "   -ffc        => FPGA Fabric Reset (deletes running content and brings Fabric in Reset State)"<<endl;
	}

	deinit();
	return 0;
}
