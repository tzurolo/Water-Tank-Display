//
//  GPS Monitor
//
//  Receives GPS Navigation data from the receiver and
//  sends position updates through the TCPIP Console
//
#ifndef GPSMONITOR_H
#define GPSMONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "CharString.h"

// sets up control pins. called once at power-up
extern void GPSMonitor_Initialize (void);

// called in each iteration of the mainloop
extern void GPSMonitor_task (void);

#endif  // GPSMONITORr_H
