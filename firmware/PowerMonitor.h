//
//  Power Monitor
//
//  Monitors mains power, and power to the pump, so we can report when it goes on and off
//
#ifndef POWERMONITOR_H
#define POWERMONITOR_H

#include <stdint.h>
#include <stdbool.h>

// prototypes for functions that clients supply to
// get notification of mains state change event
typedef void (*PowerMonitor_Notification)(const bool mainsOn, const bool pumpOn);

extern void PowerMonitor_Initialize (void);

extern void PowerMonitor_registerForNotification (
    PowerMonitor_Notification notificationFunction);

extern bool PowerMonitor_pumpOn (void);
extern bool PowerMonitor_mainsOn (void);

extern void PowerMonitor_task (void);

#endif      // PUMPMONITOR_H