//
//  SD Card
//
//  Access to SD card slot on TFT board
//
#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include <stdbool.h>

extern void SDCard_Initialize (void);

extern bool SDCard_isPresent (void);

#endif      // PUMPMONITOR_H