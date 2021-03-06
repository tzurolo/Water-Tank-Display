/*
             LUFA Library
     Copyright (C) Dean Camera, 2015.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2015  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the USBtoSerial project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "USBtoSerial.h"
#include "USBTerminal.h"
#include "EEPROMStorage.h"
#include "SystemTime.h"
#include "IOPortBitfield.h"
#include "SIM800.h"
#include "Console.h"
#include "CellularComm_SIM800.h"
#include "CellularComm_SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "TCPIPConsole.h"
#include "TFT_HXD8357D.h"
#include "Display.h"
#include "PowerMonitor.h"
#include "SDCard.h"
#include "SoftwareSerialRx0.h"
#include "SoftwareSerialTx.h"
#include "ADCManager.h"
#include "InternalTemperatureMonitor.h"
#include "WaterLevelDisplay.h"
#include "RAMSentinel.h"

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void Initialize (void)
{
    // enable watchdog timer
    wdt_enable(WDTO_500MS);

    /* Disable clock division */
//	clock_prescale_set(clock_div_1);

    EEPROMStorage_Initialize();
    SystemTime_Initialize();
    SoftwareSerialRx0_Initialize();
    SoftwareSerialTx_Initialize();
    Console_Initialize();
    CellularComm_Initialize();
    CellularTCPIP_Initialize();
    TCPIPConsole_Initialize();
    Display_Initialize();
    TFT_HXD8357D_Initialize();
    PowerMonitor_Initialize();
    SDCard_Initialize();
    ADCManager_Initialize();
    InternalTemperatureMonitor_Initialize();
    RAMSentinel_Initialize();
    USBTerminal_Initialize();
    WaterLevelDisplay_Initialize();
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
    Initialize();
    sei();
    for (;;) {
        if (!RAMSentinel_sentinelIntact()) {
            Console_printP(PSTR("stack collision!"));
            SystemTime_commenceShutdown();
        }

        // run all the tasks
        SystemTime_task();
        ADCManager_task();
        InternalTemperatureMonitor_task();
        SIM800_task();
        Console_task();
        USBTerminal_task();
        TCPIPConsole_task();
        CellularComm_task();
        PowerMonitor_task();
        WaterLevelDisplay_task();
        Display_task();
        TFT_HXD8357D_task();

        // daily reboot logic, but not in the middle of display task activity
        if (WaterLevelDisplay_taskIsIdle() &&
             (!SystemTime_shuttingDown())) {
            const uint32_t uptime = SystemTime_uptime();
            const uint32_t rebootIntervalSeconds = (((uint32_t)EEPROMStorage_rebootInterval()) * 60);
            if (uptime > rebootIntervalSeconds) {
                SystemTime_commenceShutdown();
            }
        }
    }

    return 0;
}

