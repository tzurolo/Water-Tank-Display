//
//  Display for the Water Level Display system
//
//  What it does:
//    Provides a graphical display for the water level.
//    Uses the TFT_HXD8357 display module.
//
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

extern void Display_Initialize (void);

// set water level in percent
extern void Display_setWaterLevel (
    const int8_t level,
    const uint32_t *levelTimestamp);

extern void Display_task (void);

#endif  /* DISPLAY_H */