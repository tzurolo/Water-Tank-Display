//
//  Adafruit 3.5" TFT 320x480 + Touchscreen Breakout Board w/MicroSD Socket - HXD8357D
//
//  What it does:
//    Provides initialization and drawing functions for the Adafruit TFT display, which
//    has an HXD8357D controller.
//
//  How to use it:
//    Call TFTHXD8357D_Initialize() once at the beginning of the program. Then the
//    other functions can be called to update the display
//
//  Hardware resouces used:
//     SPI pins - see SPIAsync.h
//     Data/Command (PB6)
//     Reset (PF0)
//     Lite (PF1)
//     
//
#ifndef TFTHXD8357D_H
#define TFTHXD8357D_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

extern void TFT_HXD8357D_Initialize (void);

// 0 is backlight off, 1-9 is increasing levels brightness, 10 is full on
extern void TFT_HXD8357D_setBacklightBrightness (
    const uint8_t brightness);

#endif      /* TFTHXD8357D */
