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
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "CharStringSpan.h"

// Color definitions
#define	HX8357_BLACK   0x0000
#define	HX8357_BLUE    0x001F
#define	HX8357_RED     0xF800
#define	HX8357_GREEN   0x07E0
#define HX8357_CYAN    0x07FF
#define HX8357_MAGENTA 0xF81F
#define HX8357_YELLOW  0xFFE0  
#define HX8357_WHITE   0xFFFF

#define TFT_HXD8357D_width 480
#define TFT_HXD8357D_height 320

typedef struct TFT_HXD8357D_Rectangle_struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t color;
} TFT_HXD8357D_Rectangle;

typedef struct TFT_HXD8357D_Text_struct {
    uint16_t x;
    uint16_t y;
    CharStringSpan_t chars;
    uint16_t fgColor;
    uint16_t bgColor;
} TFT_HXD8357D_Text;

// function defined by clients to specify a rectangle to draw.
// return NULL if no rectangle to draw
typedef const TFT_HXD8357D_Rectangle* (*TFT_HXD8357D_RectangleSource)(void);
// function defined by clients to specify a rectangle to draw.
// return NULL if no rectangle to draw
typedef const TFT_HXD8357D_Text* (*TFT_HXD8357D_TextSource)(void);

extern void TFT_HXD8357D_Initialize (void);

extern void TFT_HXD8357D_setRectangleSource (
    TFT_HXD8357D_RectangleSource source);

extern void TFT_HXD8357D_setTextSource (
    TFT_HXD8357D_TextSource source);

extern void TFT_HXD8357D_task (void);

// 0 is backlight off, 1-9 is increasing levels brightness, 10 is full on
extern void TFT_HXD8357D_setBacklightBrightness (
    const uint8_t brightness);

#endif      /* TFTHXD8357D */
