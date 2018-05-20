//
//  Software Serial - Receiver
//
//  What it does:
//      Software implementation of UART Receiver
//      Currently hardcoded to listen for 8N1 on pin PC2 at 4800 baud
//      Has an 80-byte queue.
//
//  How to use it:
//     Call SoftwareSerialRx0_Initialize() once at the beginning of the
//     program (upon powerup).
//     During the program you can get the byte queue using
//     SoftwareSerial_rx0Queue() to check for and read incoming bytes.
//
//  Hardware resources used:
//    Serial data in - PB4
//    AtMega328P 8-bit Timer 0
//
#ifndef SOFTWARESERIALRX0_H
#define SOFTWARESERIALRX0_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "ByteQueue.h"

// comes up enabled by default
extern void SoftwareSerialRx0_Initialize (void);

extern void SoftwareSerialRx0_enable (void);
extern void SoftwareSerialRx0_disable (void);

extern ByteQueue_t* SoftwareSerial_rx0Queue (void);

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
extern void SoftwareSerialRx0_reportHighwater (void);
#endif

#endif  /* SOFTWARESERIALRX0_H */

