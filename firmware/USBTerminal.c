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
 *  Main source file for the USBtoSerial project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "USBTerminal.h"

static const prog_char crlfP[] = {13,10,0};

/** Circular buffer to hold data from the host before it is sent to the device via the serial port. */
ByteQueue_define(16, FromUSB_Buffer,)

/** Circular buffer to hold data from the serial port before it is sent to the host. */
ByteQueue_define(150, ToUSB_Buffer,)

static bool USBConnected = false;

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};

void USBTerminal_sendCharsToHost (
    const char* text)
{
    CharStringSpan_t span;
    CharStringSpan_set(text, text + strlen(text), &span);
    USBTerminal_sendCharsToHostCSS(&span);
}

void USBTerminal_sendCharsToHostP (
    PGM_P text)
{
    // if the ring buffer fills up we simply drop the rest of the text
    PGM_P cp = text;
    char ch = 0;
    do {
        ch = pgm_read_byte(cp);
        ++cp;
        if ((ch != 0) && !ByteQueue_is_full(&ToUSB_Buffer)) {
            ByteQueue_push(ch, &ToUSB_Buffer);
        }
    } while (ch != 0);
}

void USBTerminal_sendCharsToHostCS (
    const CharString_t *text)
{
    CharStringSpan_t span;
    CharStringSpan_init(text, &span);
    USBTerminal_sendCharsToHostCSS(&span);
}

void USBTerminal_sendCharsToHostCSS (
    const CharStringSpan_t *text)
{
    CharString_Iter iter = CharStringSpan_begin(text);
    CharString_Iter end = CharStringSpan_end(text);

    // if the ring buffer fills up we simply drop the rest of the text
    while ((iter != end) && !ByteQueue_is_full(&ToUSB_Buffer)) {
        ByteQueue_push(*iter++, &ToUSB_Buffer);
    }
}

void USBTerminal_sendLineToHost (
    const char* text)
{
    USBTerminal_sendCharsToHost(text);
    USBTerminal_sendCharsToHostP(crlfP);
}

void USBTerminal_sendLineToHostP (
    PGM_P text)
{
    USBTerminal_sendCharsToHostP(text);
    USBTerminal_sendCharsToHostP(crlfP);
}

void USBTerminal_sendLineToHostCS (
    const CharString_t *text)
{
    USBTerminal_sendCharsToHostCS(text);
    USBTerminal_sendCharsToHostP(crlfP);
}

void USBTerminal_sendLineToHostCSS (
    const CharStringSpan_t *text)
{
    USBTerminal_sendCharsToHostCSS(text);
    USBTerminal_sendCharsToHostP(crlfP);
}

void USBTerminal_task (void)
{

    /* Only try to read in bytes from the CDC interface if the transmit buffer is not full */
    if (!(ByteQueue_is_full(&FromUSB_Buffer))) {
	int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

	/* Read bytes from the USB OUT endpoint into the USART transmit buffer */
	if (!(ReceivedByte < 0)) {
	    ByteQueue_push(ReceivedByte, &FromUSB_Buffer);
        }
    }

    /* Check if there is any data in the ToUSB buffer */
    uint16_t BufferCount = ByteQueue_length(&ToUSB_Buffer);
    if (BufferCount > 0) {
        Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);

        /* Check if a packet is already enqueued to the host - if so, we shouldn't try to send more data
	    * until it completes as there is a chance nothing is listening and a lengthy timeout could occur */
        if (Endpoint_IsINReady())
        {
            /* Never send more than one bank size less one byte to the host at a time, so that we don't block
	        * while a Zero Length Packet (ZLP) to terminate the transfer is sent if the host isn't listening */
            uint8_t BytesToSend = MIN(BufferCount, (CDC_TXRX_EPSIZE - 1));

            /* Read bytes from the USART receive buffer into the USB IN endpoint */
            while (BytesToSend--) {
                /* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
                if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface,
                        ByteQueue_head(&ToUSB_Buffer)) != ENDPOINT_READYWAIT_NoError) {
                    break;
                }

                /* Dequeue the already sent byte from the buffer now we have confirmed that no transmission error occurred */
                ByteQueue_pop(&ToUSB_Buffer);
            }
        }
    }

    CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
    USB_USBTask();
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void USBTerminal_Initialize (void)
{
    ByteQueue_clear(&FromUSB_Buffer);
    ByteQueue_clear(&ToUSB_Buffer);

    USB_Init();
}

bool USBTerminal_isConnected (void)
{
	return USBConnected;
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	USBConnected = true;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	USBConnected = false;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool ConfigSuccess = true;

    ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
    CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
    // don't change the UART settings. We are using the UART to talk to the
    // cellular module at 115200 8N1
#if 0
    uint8_t ConfigMask = 0;

    switch (CDCInterfaceInfo->State.LineEncoding.ParityType) {
	case CDC_PARITY_Odd:
	    ConfigMask = ((1 << UPM11) | (1 << UPM10));
	    break;
	case CDC_PARITY_Even:
	    ConfigMask = (1 << UPM11);
	    break;
	}

	if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	switch (CDCInterfaceInfo->State.LineEncoding.DataBits) {
	    case 6:
	        ConfigMask |= (1 << UCSZ10);
	        break;
	    case 7:
		ConfigMask |= (1 << UCSZ11);
		break;
	    case 8:
		ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
		break;
	}

	/* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;

	/* Set the new baud rate before configuring the USART */
	UBRR1  = SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

	/* Reconfigure the USART in double speed mode for a wider baud rate range at the expense of accuracy */
	UCSR1C = ConfigMask;
	UCSR1A = (1 << U2X1);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));
#endif
}

