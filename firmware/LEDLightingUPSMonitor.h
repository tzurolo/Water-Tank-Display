//
//  LED Lighting UPS Monitor
//
//  Continually polls the LED Lighting UPS for current status and
//  sends updates through the TCPIP Console
//
#ifndef LEDLightingUPSMonitor_H
#define LEDLightingUPSMonitor_H

#include <stdint.h>
#include <stdbool.h>
#include "CharString.h"

typedef enum UPSStatus_enum {
    upss_unknown = -1,
    upss_poweringDown = 0,
    upss_onBattery = 1,
    upss_onACAdapter = 2
} UPSStatus;

// sets up control pins. called once at power-up
extern void LEDLightingUPSMonitor_Initialize (void);

// called in each iteration of the mainloop
extern void LEDLightingUPSMonitor_task (void);

extern void LEDLightingUPSMonitor_sendCommand (
    const char *command);

extern const CharString_t* LEDLightingUPSMonitor_UPSStatusStr (void);
extern const CharString_t* LEDLightingUPSMonitor_UPSSettings (void);

// returns voltage in units of 1/100 V
extern int16_t LEDLightingUPSMonitor_UPSVoltage (void);
extern int16_t LEDLightingUPSMonitor_BatteryVoltage (void);
extern int16_t LEDLightingUPSMonitor_ACAdapterVoltage (void);

extern UPSStatus LEDLightingUPSMonitor_UPSStatus (void);

#endif  // LEDLightingUPSMonitor_H
