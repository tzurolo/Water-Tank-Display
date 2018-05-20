//
//  Console
//
//  This unit responds to commands from the USB host
//
#ifndef CONSOLE_H
#define CONSOLE_H

#include "CharStringSpan.h"

// sets up control pins. called once at power-up
extern void Console_Initialize (void);

// reads commands from FromUSB_Buffer and writes responses to
// ToUSB_Buffer.
// called in each iteration of the mainloop
extern void Console_task (void);

extern void Console_print (
    const char* text);

extern void Console_printP (
    PGM_P text);

extern void Console_printCS (
    const CharString_t *text);

extern void Console_printCSS (
    const CharStringSpan_t *text);

#endif  // Console_H
