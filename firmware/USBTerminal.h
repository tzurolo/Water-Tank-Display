/*
             LUFA Library
     Copyright (C) Dean Camera, 2012.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2012  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
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
 *  Header file for USBtoSerial.c.
 */

#ifndef _USBTERMINAL_H_
#define _USBTERMINAL_H_

    /* Includes: */
        #include <avr/io.h>
        #include <avr/wdt.h>
        #include <avr/interrupt.h>
        #include <avr/power.h>
        #include <avr/pgmspace.h>

        #include "Descriptors.h"

        #include <LUFA/Version.h>
        #include <LUFA/Drivers/Peripheral/Serial.h>
        #include <LUFA/Drivers/USB/USB.h>
        #include "CharStringSpan.h"
        #include "ByteQueue.h"

        extern ByteQueue_t FromUSB_Buffer;
        extern ByteQueue_t ToUSB_Buffer;

    /* Function Prototypes: */
        void USBTerminal_sendCharsToHost (
            const char* text);
        void USBTerminal_sendCharsToHostP (
            PGM_P text);
        void USBTerminal_sendCharsToHostCS (
            const CharString_t *text);
        void USBTerminal_sendCharsToHostCSS (
            const CharStringSpan_t *text);

        void USBTerminal_sendLineToHost (
            const char* text);
        void USBTerminal_sendLineToHostP (
            PGM_P text);
        void USBTerminal_sendLineToHostCS (
            const CharString_t *text);
        void USBTerminal_sendLineToHostCSS (
            const CharStringSpan_t *text);

        void USBTerminal_task(void);
        void USBTerminal_Initialize (void);
        bool USBTerminal_isConnected (void);

        void EVENT_USB_Device_Connect(void);
        void EVENT_USB_Device_Disconnect(void);
        void EVENT_USB_Device_ConfigurationChanged(void);
        void EVENT_USB_Device_ControlRequest(void);

        void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo);

#endif

