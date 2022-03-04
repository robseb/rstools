/**
 *  
 * @file    main.cpp
 * @brief   FPGA-status
 * @author  rsyocto GmbH & Co. KG 
 * 			Robin Sebastian (git@robseb.de)
 * @mainpage
 * rstools application to read the Status of the FPGA Fabric
 * and the Bridges between the HPS and FPGA 
 * 
 * Chnage Log: 
 * 		1.00 (03-03-2022)
 * 		Initial release
 * 
 * Copyright (C) 2021-2022 rsyocto GmbH & Co. KG  *  All Rights Reserved
 * 
 */

#define VERSION "1.00"


#include <iostream>
#include <fstream>					// POSIX: for acessing Linux drivers
#include <sys/mman.h>				// POSIX: memory maping
#include <fcntl.h>					// POSIX: "PROT_WRITE", "MAP_SHARED", ...
#include <unistd.h>					// POSIX: for closing the Linux driver access
#include <cstdint>                  // Standard integral types (uint8_t,...)


using namespace std;

/*
*
* Registers
*/

// FPGA Manager Status Register (MSEL, Status)
#define REG_FPGAMG_STATUS			0xFF706000
#define REG_FPGAMG_STATUS_OFFSET	0x0

// System Mangaer bootinfo Register
#define REG_SYSMAN_BASE				0xFFD08000
#define REG_SYSMAN_SILID_OFFSET		0x00
#define REG_SYSMAN_BOOTINFO			0xFFD08014
#define REG_SYSMAN_BOOTINFO_OFFSET	0x14
#define REG_SYSMAN_HPSINFO			0xFFD08018
#define REG_SYSMAN_HPSINFO_OFFSET	0x18
#define REG_SYSMAN_GBL				0xFFD08020
#define REG_SYSMAN_GBL_OFFSET		0x20
#define REG_SYSMAN_INDIV			0xFFD08024
#define REG_SYSMAN_INDIV_OFFSET		0x24
#define REG_SYSMAN_MODULE			0xFFD08028
#define REG_SYSMAN_MODULE_OFFSET	0x28


#define REG_WDT0_BASE				0xFFD02000
#define REG_WDT0_OFFFSET			0x00

#define REG_WDT1_BASE				0xFFD03000
#define REG_WDT1_OFFFSET			0x00


#define REG_CLCK_CTRL				0xFFD04000
#define REG_CLCK_CTRL_OFFFSET		0x00
/*
* Global Values
*/

volatile int posix_fd;
volatile void* fpgaMangerMap;    	// Memory Map assigned to FPGA Manger
volatile uint32_t* ptrFpgaManger;   // Pointer to FPGA Manager Registers 

volatile void* systemMangerMap;    	// Memory Map assigned to System Manager
volatile uint32_t* ptrsystemManger; // Pointer to the System Manager

volatile void* wdt0Map;    			
volatile uint32_t* ptrwdt0; 

volatile void* wdt1Map;    			
volatile uint32_t* ptrwdt1;

volatile void* clkmgrMap;    			
volatile uint32_t* ptrclkmgr;

/*
*   @brief               Convert MSEL Code to String with mode description 
*   @param	msel_code	 MSEL Code (acording datasheet)
*   @return              description string
*/ 
string msel2str(uint8_t msel_code)
{
	switch (msel_code)
	{
	case 0x00:
		return "		0x00 16-bit Passive Parallel (16PP) with Fast Power on Reset on Reset Delay;\n" \
			   "			 No AES Encryption; No Data Compression. CDRATIO must be programmed to x1";
	case 0x01:
		return "		0x01 16-bit Passive Parallel (16PP) with Fast Power on Reset on Reset Delay;\n" \
			   "			 With AES Encryption; No Data Compression. CDRATIO must be programmed to x2";
	case 0x02:
		return "		0x02 16-bit Passive Parallel (16PP) with Fast Power on Reset on Reset Delay;\n" \
			   "			 AES Optional; With Data Compression. CDRATIO must be programmed to x4";
	case 0x04:
		return "		0x04 16-bit Passive Parallel (16PP) with Slow Power on Reset Delay;\n" \
			   "			 No AES Encryption; No Data Compression. CDRATIO must be programmed to x1";
	case 0x05:
		return "		0x05 16-bit Passive Parallel (16PP) with Slow Power on Reset Delay;\n" \
			   "			 With AES Encryption; No Data Compression. CDRATIO must be programmed to x2";
	case 0x06:
		return "		0x06 16-bit Passive Parallel (16PP) with Slow Power on Reset Delay;\n" \
			   "		     With AES Optional; With Data Compression. CDRATIO must be programmed to x4";
	case 0x08:
		return "		0x08 32-bit Passive Parallel (32PP) with Fast Power on Reset on Reset Delay;\n" \
			   "		     No AES Encryption; With Data Compression. CDRATIO must be programmed to x1";
	case 0x09:
		return "		0x09 32-bit Passive Parallel (32PP) with Fast Power on Reset on Reset Delay;\n" \
			   "		     With AES Encryption; With Data Compression. CDRATIO must be programmed to x4";
	case 0x0a:
		return "		0x0a 32-bit Passive Parallel (32PP) with Fast Power on Reset on Reset Delay;\n" \
			   "		     AES Optional; With Data Compression. CDRATIO must be programmed to x8";
	case 0x0c:
		return "		0x0c 32-bit Passive Parallel (32PP) with Slow Power on Reset on Reset Delay;\n" \
			   "		     No AES Encryption; With Data Compression. CDRATIO must be programmed to x1";
	case 0x0d:
		return "		0x0d 32-bit Passive Parallel (32PP) with Slow Power on Reset on Reset Delay;\n" \
			   "		     With AES Encryption; No Data Compression. CDRATIO must be programmed to x4";
	case 0x0e:
		return "		0x0e 32-bit Passive Parallel (32PP) with Slow Power on Reset on Reset Delay;\n" \
			   "		     AES Optional; With Data Compression. CDRATIO must be programmed to x8";
	default:
		return "		     ERROR MSEL (MODE SELECT) POSITION IS UNKNWON!\n" \
			   "		     ";
	}
	return "";
}

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
*   @brief               Convert Bode Select Code to description string
*   @param	bsel_code	 BSEL (acording datasheet)
*   @return              description string
*/ 
string bsl2str(uint8_t bsel_code)
{
	switch (bsel_code)
	{
	case 0x00:
		return "		0x00 Reserved";
	case 0x01:
		return "		0x01 FPGA (HPS2FPGA Bridge)";
	case 0x02:
		return "		0x02 NAND Flash (1.8v)";
	case 0x03:
		return "		0x03 NAND Flash (3.0v)";
	case 0x04:
		return "		0x04 SD/MMC External Transceiver (1.8v)";
	case 0x05:
		return "		0x05 SD/MMC External Transceiver (3.0v)";
	case 0x06:
		return "		0x06 QSPI Flash (1.8v)";
	case 0x07:
		return "		0x07 QSPI Flash (3.0v)";
	default:
		return "	   ERROR BSEL IS UNKNWON!";
	}
	return "";
}

/*
*   @brief               Convert System Manger Indiv Register to description string
*   @param	indiv_code	 indiv code (acording datasheet)
*   @return              description string
*/ 
string indiv2str(uint8_t indiv_code)
{	
	string msg="";
	if(indiv_code & (1<<0)) 
		msg = "		L 	[Y] Reset request interface is enabled. Logic in the FPGA fabric can reset the HPS.\n";
	else
		msg = "		L 	[N] Reset request interface is disabled. Logic in the FPGA fabric cannot reset the HPS.\n"; 	
	
	if(indiv_code & (1<<1)) 
		msg =msg+ "		L 	[Y] Enables the fpgajtagen bit found in the ctrl register.\n";
	else
		msg =msg+ "		L	[N] Disables the fpgajtagen bit found in the ctrl register.\n"; 	
	
	if(indiv_code & (1<<2)) 
		msg =msg+ "		L 	[Y] CONFIG_IO interface is enabled. Execution of the CONFIG_IO instruction \n"+\
				  "            		in the FPGA JTAG TAP controller is supported.\n";
	else
		msg =msg+ "		L	[N] CONFIG_IO interface is disabled. Execution of the CONFIG_IO instruction in the FPGA JTAG TAP controller\n"+ \
				  "           		is unsupported and produces undefined results.\n"; 	

	if(indiv_code & (1<<3)) 
		msg =msg+ "		L 	[Y] Boundary-scan interface is enabled. Execution of the boundary-scan instructions \n"+\
				  "            		in the FPGA JTAG TAP controller is supported.\n";
	else
		msg =msg+ "		L 	[N] Boundary-scan interface is disabled. Execution of boundary-scan instructions in the FPGA JTAG TAP\n"+ \
				  "            		controller is unsupported and produces undefined results.\n"; 	

	if(indiv_code & (1<<4)) 
		msg =msg+ "		L	[Y] Trace interface is enabled. Other registers in the HPS debug logic must be programmmed to\n" +\
				  " 		 actually send trace data to the FPGA fabric.\n";
	else
		msg =msg+ "		L 	[N] Trace interface is disabled. HPS debug logic cannot send trace data to the FPGA fabric.\n";

	if(indiv_code & (1<<6)) 
		msg =msg+ "		L 	[Y] STM event interface is enabled. Logic in the FPGA fabric can trigger STM events.\n";
	else
		msg =msg+ "		L	[N] STM event interface is disabled. Logic in the FPGA fabric cannot trigger STM events.\n";
	
	if(indiv_code & (1<<7)) 
		msg =msg+ "		L 	[Y] FPGA Fabric can send triggers.\n";
	else
		msg =msg+ "		L 	[N] FPGA Fabric cannot send triggers.\n";

	return msg;				
}

/*
*   @brief               Convert System Manger Module Register to description string
*   @param	indiv_code	 register (acording datasheet)
*   @return              description string
*/ 
string moudleEn2str(uint8_t regitser)
{	
	string msg="";
	if(regitser & (1<<0)) 
		msg = "		L 	[Y] Enable signals from FPGA fabric to HPS SPIM0 module interface.\n";
	else
		msg = "		L 	[N] Disable signals from FPGA fabric to HPS SPIM0 module interface.\n"; 
	
	if(regitser & (1<<1)) 
		msg =msg+= "		L 	[Y] Enable signals from FPGA fabric to HPS SPIM1 module interface.\n";
	else
		msg =msg+= "		L 	[N] Disable signals from FPGA fabric to HPS SPIM1 module interface.\n"; 	
	
	if(regitser & (1<<2)) 
		msg =msg+= "		L 	[Y] Enable signals from FPGA fabric to HPS EMAC0 module interface.\n";
	else
		msg =msg+= "		L 	[N] Disable signals from FPGA fabric to HPS EMAC0 module interface.\n"; 

	if(regitser & (1<<3)) 
		msg =msg+= "		L 	[Y] Enable signals from FPGA fabric to HPS EMAC1 module interface.\n";
	else
		msg =msg+= "		L 	[N] Disable signals from FPGA fabric to HPS EMAC1 module interface.\n"; 

	if(regitser & (1<<5)) 
		msg =msg+= "		L 	[Y] Enable signals from FPGA fabric to HPS SD/MMC controller module interface.\n";
	else
		msg =msg+= "		L 	[N] Disable signals from FPGA fabric to HPS SD/MMC controller module interface.\n"; 	
	

	return msg;	
}


/*
*   @brief               Convert Clock Controll Register to description string
*   @param	indiv_code	 register (acording datasheet)
*   @return              description string
*/ 
string clockCtrl2str(uint8_t regitser)
{	
	string msg="";
	if(regitser & (1<<0)) 
		msg = "		L 	[Y] Safe Mode Enabled!: Main PLL hardware-managed clocks are bypassed and osc1_clk is used\n";
	else
		msg = "		L 	[N] Safe Mode Disabled: Hardware-managed clock are in use\n"; 
	
	if(regitser & (1<<2)) 
		msg =msg+= "		L 	[Y] After the warm reset, Safe Mode will be activated automatically\n";
	else
		msg =msg+= "		L 	[N] After the warm reset, Safe Mode will not enabled!\n"; 

	return msg;	
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

	
	//
	// System Manger Address Space
	//

    // Create a Memory Map to acess the System Manger
	systemMangerMap = mmap(NULL,REG_SYSMAN_MODULE_OFFSET+4, PROT_READ, MAP_PRIVATE, posix_fd, \
		REG_SYSMAN_BASE);

    if (systemMangerMap == MAP_FAILED)
	{
		cout << "\n[ERROR] Failed to open the memory maped interface to the System Manager" << endl;
        return -1;
	}

    // Allocate a pointer to the maped address space 
    ptrsystemManger = (uint32_t*)systemMangerMap;

    if (ptrsystemManger==0) 
    {
        cout << "\n[ERROR] Failed to allocate a ponter to the to the System Manager Address Space"<<endl;
        return -1;
    }
	
	//
	// WatchDog 0 Address Space
	//

    // Create a Memory Map
	wdt0Map = mmap(NULL,4, PROT_READ, MAP_PRIVATE, posix_fd, \
		REG_WDT0_BASE);

    if (wdt0Map == MAP_FAILED)
	{
		cout << "\n[ERROR] Failed to open the memory maped interface to the WatchDog 0" << endl;
        return -1;
	}

    // Allocate a pointer to the maped address space 
    ptrwdt0 = (uint32_t*)wdt0Map;

    if (ptrwdt0==0) 
    {
        cout << "\n[ERROR] Failed to allocate a ponter to the to the WatchDog 0"<<endl;
        return -1;
    }
	
	//
	// WatchDog 1 Address Space
	//

    // Create a Memory Map
	wdt1Map = mmap(NULL,4, PROT_READ, MAP_PRIVATE, posix_fd, \
		REG_WDT1_BASE);

    if (wdt1Map == MAP_FAILED)
	{
		cout << "\n[ERROR] Failed to open the memory maped interface to the WatchDog 1" << endl;
        return -1;
	}

    // Allocate a pointer to the maped address space 
    ptrwdt1 = (uint32_t*)wdt1Map;

    if (ptrwdt1==0) 
    {
        cout << "\n[ERROR] Failed to allocate a ponter to the to the WatchDog 1"<<endl;
        return -1;
    }


	//
	// Clock Manager Address Space
	//

    // Create a Memory Map
	clkmgrMap = mmap(NULL,4, PROT_READ, MAP_PRIVATE, posix_fd, \
		REG_CLCK_CTRL);

    if (clkmgrMap == MAP_FAILED)
	{
		cout << "\n[ERROR] Failed to open the memory maped interface to the Clock Manager" << endl;
        return -1;
	}

    // Allocate a pointer to the maped address space 
    ptrclkmgr = (uint32_t*)clkmgrMap;

    if (ptrclkmgr==0) 
    {
        cout << "\n[ERROR] Failed to allocate a ponter to the to the Clock Manager"<<endl;
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

	if (wdt0Map != MAP_FAILED)
	{
		munmap((void*) wdt0Map, REG_WDT0_BASE);
	}

	if (wdt1Map != MAP_FAILED)
	{
		munmap((void*) wdt1Map, REG_WDT1_BASE);
	}

	if (clkmgrMap != MAP_FAILED)
	{
		munmap((void*) clkmgrMap, REG_CLCK_CTRL);
	}

    // close POSIX port
    close(posix_fd);
}


/*
*   @brief              Read FPGA MSEL (FPGA Configuration Mode) Selection 
*   @return             MSEL Code         
*/ 
uint8_t readMSEL(void)
{	
	if (ptrFpgaManger==0) return 0;
	// Read Bit 3-7 of the FPGA Manager Status register
	uint32_t stat = *(ptrFpgaManger + (REG_FPGAMG_STATUS_OFFSET)) & 0xF8;
	return (uint8_t) (stat >> 3); 
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
*   @brief              Read BSEL (Boot Select Register)
*   @return             BSEL Code         
*/ 
uint8_t readBSL(void)
{	
	if (ptrsystemManger==0) return 255;
	// Read Bit 0-3 of the System Manager Boot Configuration Register
	uint32_t bsl = *(ptrsystemManger + (REG_SYSMAN_BOOTINFO_OFFSET/4)) & 0x7;
	return (uint8_t) bsl; 
}


/*
*   @brief              Read if the Device has CAN Bus
*   @return             has CAN0 and CAN1          
*/ 
uint8_t hasCan(void)
{	
	if (ptrsystemManger==0) return 255;
	// Read Bit 1 of the System Manager HPS Register
	uint32_t can = *(ptrsystemManger + (REG_SYSMAN_HPSINFO_OFFSET/4)) & 0b01;
	return (uint8_t) can; 
}

/*
*   @brief              Read if the HPS has a Dual-Core CPU
*   @return             has Dual-Core         
*/ 
uint8_t isDualCore(void)
{	
	if (ptrsystemManger==0) return 255;
	// Read Bit 1 of the System Manager HPS Register
	uint32_t dualcore = *(ptrsystemManger + (REG_SYSMAN_HPSINFO_OFFSET/4)) & 0b1;
	return (uint8_t) dualcore; 
}

/*
*   @brief              Read if the global interface between FPGA and HPS disabled
*   @return             0  	All interfaces between FPGA and HPS are disabled.
*/ 
uint8_t isGlobalInterfaceEnbaled(void)
{	
	if (ptrsystemManger==0) return 255;
	return *(ptrsystemManger + (REG_SYSMAN_GBL_OFFSET/4)) & 0b1;
}


/*
*   @brief              Get System Manager indiv Register
*   @return             indiv register [7:0]
*/ 
uint8_t readSysManIndiv(void)
{	
	if (ptrsystemManger==0) return 255;
	return *(ptrsystemManger + (REG_SYSMAN_INDIV_OFFSET/4)) & 0xFF;
}

/*
*   @brief              Read the Silicon Revison Number
*   @return             Silicion Revison Number       
*/ 
uint16_t readSiliconRev(void)
{	
	if (ptrsystemManger==0) return 255;
	uint32_t rev = *(ptrsystemManger + (REG_SYSMAN_SILID_OFFSET/4)) & 0xFFFF;
	return (uint16_t) rev; 
}

/*
*   @brief              Read the Silicon ID
*   @return             Silicion ID     
*/ 
uint16_t readSiliconID(void)
{	
	if (ptrsystemManger==0) return 255;
	uint32_t id = *(ptrsystemManger + (REG_SYSMAN_SILID_OFFSET/4)) & 0xFFFF0000;
	return (uint16_t) (id >> 16); 
}

/*
*   @brief              Read module Signal Enable Register
*   @return             Register [5:0]  
*/ 
uint8_t readModuleSignalEn(void)
{	
	if (ptrsystemManger==0) return 255;
	uint32_t module = *(ptrsystemManger + (REG_SYSMAN_MODULE_OFFSET/4)) & 0x1F;;
	return (uint8_t) module; 
}

/*
*   @brief              Read the Clock Manger Controll Register
*   @return             indiv register [7:0]
*/ 
uint8_t readClockCtrl(void)
{	
	if (ptrclkmgr==0) return 255;
	return *(ptrclkmgr + (REG_CLCK_CTRL_OFFFSET/4)) & 0x7;
}


/*
*   @brief              Check if WatchDog 0 is enbaled
*   @return             WatchDog 0 enbaled     
*/ 
uint8_t WatchDog0Enabled(void)
{	
	if (ptrwdt0==0) return 0;
	uint32_t wdt = *(ptrwdt0 + (REG_WDT0_OFFFSET/4));
	return (uint8_t) ((wdt & (1<<0)) && !(wdt & (1<<1)));
}

/*
*   @brief              Check if WatchDog 1 is enbaled
*   @return             WatchDog 1 enbaled     
*/ 
uint8_t WatchDog1Enabled(void)
{	
	if (ptrwdt1==0) return 0;
	uint32_t wdt = *(ptrwdt1 + (REG_WDT1_OFFFSET/4));
	return (uint8_t) ((wdt & (1<<0)) && !(wdt & (1<<1)));
}

int main(int argc, const char* argv[])
{

	if(initMemRegs()==-1) return -1;

	uint8_t  msel_code 	 	= readMSEL();
	uint8_t  state_code  	= readState();
	uint8_t  bsel_code   	= readBSL();
	uint8_t  has_can	 	= hasCan();
	uint8_t  is_dualcore 	= isDualCore();
	uint16_t silicon_rev 	= readSiliconRev();
	uint16_t silicon_id  	= readSiliconID();
	uint8_t  watchDog0_en   = WatchDog0Enabled();
	uint8_t  watchDog1_en   = WatchDog1Enabled();
	uint8_t  global_inf_en  = isGlobalInterfaceEnbaled();
	uint8_t  indiv_code		= readSysManIndiv();
	uint8_t  signal_en		= readModuleSignalEn();
	uint8_t  clock_ctrl		= readClockCtrl();
	
	string stat ="";

	if ((argc > 1) && (std::string(argv[1]) == "-h"))
	{
		cout << "	Command to read current Status of the HPS and FPGA Fabric" << endl;
		cout << "	FPGA-status" << endl;
		cout << "		Read the status with detailed output" << endl;
		cout <<endl <<"Vers.: "<<VERSION<<endl;
		cout <<"Copyright (C) 2021-2022 rsyocto GmbH & Co. KG" << endl;

	}
	else
	{
		// Print the MSEL Value as detailed string 
		cout << "-- Reading the Status of the FPGA Fabric --" << endl;

		cout << "# MSEL (Mode Select) Position:" <<endl;
		cout << msel2str(msel_code) << endl;

		cout << "# FPGA Fabric State:" <<endl;
		cout << state2str(state_code) << endl;

		cout << "# HPS Boot Select (BSEL):" <<endl;
		cout << bsl2str(bsel_code) << endl;

		cout << "# HPS Info:" <<endl;
		if (is_dualcore==1) cout << "	        [Y] Is dual-core (CPU0 and CPU1 both available)."<<endl;
		else				cout << "	        [N] Not dual-core (only CPU0 available)."<<endl;
		
		if (has_can==1) 	cout << "	        [Y] CAN0 and CAN1 are available"<<endl;
		else				cout << "	        [N] CAN0 and CAN1 are not available"<<endl;
		
		cout << "	        Silicon revision No: "<<silicon_rev;
		if 		(silicon_rev==0x1) cout << " (First Silicon)"<<endl;
		else if (silicon_rev==0x2) cout << " (Silicon with L2 ECC fix)"<<endl;
		else if (silicon_rev==0x1) cout << " (Silicon with HPS PLL (warm reset) fix)"<<endl;
		else cout <<endl;

		cout << "	        Silicon ID: "<<silicon_id <<endl;
		
		cout << "# WatchDog Status:" <<endl;

		if (watchDog0_en==1) 	cout << "	  L     [Y] Watchdog 0 enabled and generates a warm reset request"<<endl;
		else					cout << "	  L     [N] Watchdog 0 disabled"<<endl;

		if (watchDog1_en==1) 	cout << "	  L     [Y] Watchdog 1 enabled and generates a warm reset request"<<endl;
		else					cout << "	  L     [N] Watchdog 1 disabled"<<endl;

		cout << "#  Interfaces/Signals between the FPGA and HPS:" <<endl;

		if (global_inf_en==1) 	cout << "	   L    [Y] Interfaces between FPGA and HPS are not all global disabled"<<endl;
		else					cout << "	   L    [N] [INTERFACE GLOBAL RESET] All interfaces between FPGA and HPS are disabled."<<endl;
		
		cout << "	        General Signals of the HPS Module"<<endl;
		cout << indiv2str(indiv_code)<<endl;
		cout << "	        Specific module signals Enabled/Disabled"<<endl;
		cout << moudleEn2str(signal_en)<<endl;

		cout << "#  Clock Manager Settings" <<endl;
		cout << clockCtrl2str(clock_ctrl)<< endl;
	}

	deinit();
	return 0;
}
