//
//  SPI asynchronous interface
//
//  What it does:
//    Interface to AVR hardware SPI. The functions in this unit return immediately.
//    Use SPIAsync_operationCompleted to determine if sendByte or requestByte
//    have completed. This allows SPI transactions to be in progress while other
//    processing takes place (doesn't block waiting for operations to complete).
//    Based on AtMega32U4 spec
//
//  How to use it:
//    Call SPIAsync_init() once at the beginning of the program. Then the
//    other functions can be called to send and receive data.
//
//  Hardware resouces used:
//     SCK pin (PB1)
//     MOSI pin (PB2)
//     MISO pin (PB3)
//     Normally uses PB0 as SS, but PB0 is not available on the Feather Fona, so using PD6
//
#ifndef SPIASYNC_H
#define SPIASYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/io.h>
#define SPIAsync_DBLSPD (1 << 8)

// mode options
#define SPIAsync_MODE_SLAVE     (0 << MSTR)
#define SPIAsync_MODE_MASTER    (1 << MSTR)

// speed options
#define SPIAsync_SPEED_FOSC_DIV_2   SPIAsync_DBLSPD
#define SPIAsync_SPEED_FOSC_DIV_4   0
#define SPIAsync_SPEED_FOSC_DIV_8   (SPIAsync_DBLSPD | (1 << SPR0))
#define SPIAsync_SPEED_FOSC_DIV_16  (1 << SPR0)
#define SPIAsync_SPEED_FOSC_DIV_32  (SPIAsync_DBLSPD | (1 << SPR1))
#define SPIAsync_SPEED_FOSC_DIV_64  (1 << SPR1)
#define SPIAsync_SPEED_FOSC_DIV_128 ((1 << SPR1) | (1 << SPR0))

// clock polarity options
#define SPIAsync_SCK_LEAD_RISING    (0 << CPOL)
#define SPIAsync_SCK_LEAD_FALLING   (1 << CPOL)

// clock phase options
#define SPIAsync_SCK_LEAD_SAMPLE    (0 << CPHA)
#define SPIAsync_SCK_LEAD_SETUP     (1 << CPHA)

// bit order options
#define SPIAsync_ORDER_MSB_FIRST    (0 << DORD)
#define SPIAsync_ORDER_LSB_FIRST    (1 << DORD)

// initializes SPI system with the given options. options is a bit mask
// of the options macros defined above. Note that this sets the SS
// pin (PB0) to be an output and sets it high
extern void SPIAsync_init (const uint16_t options);

// for master mode assert SS pin (PB0) to alert slave
extern void SPIAsync_assertSS (void);
// for master mode deassert SS pin (PB0) to tell slave we're done
extern void SPIAsync_deassertSS (void);

// sends one byte asynchronously. Call SPIAsync_operationCompleted to
// determine if the transmission has been completed.
// Note that this will also read a byte from the slave device - after
// transmission has completed you can call SPIAsync_readByte.
extern void SPIAsync_sendByte (
    const uint8_t byte);

// requests one byte asynchronously. Call SPIAsync_operationCompleted to
// determine if the byte has been received. When it returns true then
// call SPIAsync_readByte to get the data
extern void SPIAsync_requestByte (void);

// reads the SPI data register after a byte has been received. Before
// calling this function you must first call SPIAsync_requestByte
// Intended to be used something like this:
//   SPIAsync_requestByte();
//   while (!SPIAsync_operationCompleted()); // wait for byte
//   uint8_t readData = SPIAsync_getByte()
extern uint8_t SPIAsync_getByte (void);

// call this function after SPI_sendByte or SPI_requestByte to determine
// if the operation has completed
extern bool SPIAsync_operationCompleted (void);

#endif  // SPIASYNC
