//
//  SPI interface
//  for Adafruit Feather Fona
//

#include "SPIAsync.h"

// on Adafruit Feather Fona the usual SS pin (PB0) is not available. Using PD6
#define SS_OUTPORT  PORTD
#define SS_PIN      PD6
#define SS_DIR      DDRD


void SPIAsync_init (const uint16_t options)
{
    // make SS pin an unasserted output
    SS_OUTPORT |= (1 << SS_PIN);
    SS_DIR  |= (1 << SS_PIN);

    // we can't use PB0 as SS because it's not connected on the Feather Fona
    // but we must set it to 1 for SPI master mode to work.
    PORTB |= (1 << PB0);

    // make SCK and MOSI pins (PB1 and PB2) outputs
    DDRB  |=  ((1 << PB1) | (1 << PB2));

    // make MISO pin (PB3) an input and set it high.
    DDRB  &= ~(1 << PB3);
    PORTB |=  (1 << PB3);

    // set doublespeed bit
    if ((options & SPIAsync_DBLSPD) != 0) {
        SPSR |= (1 << SPI2X);
    } else {
        SPSR &= ~(1 << SPI2X);
    }

    // enable SPI and set other options
    SPCR = ((1 << SPE) | (uint8_t)((options & 0xFF)));
}

void SPIAsync_assertSS (void)
{
    SS_OUTPORT &= ~(1 << SS_PIN);
}

void SPIAsync_deassertSS (void)
{
    SS_OUTPORT |= (1 << SS_PIN);
}

void SPIAsync_sendByte (
    const uint8_t byte)
{
    // Start transmission
    SPDR = byte;
}

void SPIAsync_requestByte (void)
{
    // Send dummy byte to get return byte
    SPDR = 0;
}

uint8_t SPIAsync_getByte (void)
{
    return SPDR;
}

// call this function after SPI_sendByte or SPI_requestByte to determine
// if the operation has completed
bool SPIAsync_operationCompleted (void)
{
    return (SPSR & (1<<SPIF));
}
