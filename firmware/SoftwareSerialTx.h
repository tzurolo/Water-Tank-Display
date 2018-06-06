//
//  Software Serial Transmit
//
//  Software implementation of multi-channel UART Transmitter
//
//  2 channels max at present
//
#ifndef SOFTWARESERIALTX_H
#define SOFTWARESERIALTX_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/pgmspace.h>
#include "CharStringSpan.h"
#include "IOPortBitfield.h"
#include "ByteQueue.h"

extern void SoftwareSerialTx_Initialize (void);

extern void SoftwareSerialTx_open (
    const uint8_t channelIndex,
    const IOPortBitfield_PortSelection port,
    const uint8_t pin);

extern void SoftwareSerialTx_enable (
    const uint8_t channelIndex);
extern void SoftwareSerialTx_disable (
    const uint8_t channelIndex);

extern bool SoftwareSerialTx_isIdle (
    const uint8_t channelIndex);

extern uint16_t SoftwareSerialTx_availableSpace (
    const uint8_t channelIndex);

extern void SoftwareSerialTx_send (
    const uint8_t channelIndex,
    const char* text);

extern void SoftwareSerialTx_sendCS (
    const uint8_t channelIndex,
    const CharString_t* text);

extern void SoftwareSerialTx_sendCSS (
    const uint8_t channelIndex,
    const CharStringSpan_t* text);

extern bool SoftwareSerialTx_sendP (
    const uint8_t channelIndex,
   PGM_P string);

extern void SoftwareSerialTx_sendChar (
    const uint8_t channelIndex,
    const char ch);

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
extern void SoftwareSerialTx_reportHighwater (void);
#endif

#endif  /* SOFTWARESERIALTX_H */

