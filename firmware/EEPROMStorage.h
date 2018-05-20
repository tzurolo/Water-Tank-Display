//
// EEPROM Storage
//
// Storage of non-volatile settings and data
//

#ifndef EEPROMSTORAGE_H
#define EEPROMSTORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "CharStringSpan.h"

#define EEPROMStorage_maxNotificationNumbers 4

extern void EEPROMStorage_Initialize (void);

extern void EEPROMStorage_setUnitID (
    const uint16_t id);
extern uint16_t EEPROMStorage_unitID (void);

extern void EEPROMStorage_setLastRebootTimeSec (
    const uint32_t sec);
extern uint32_t EEPROMStorage_lastRebootTimeSec (void);

// how long to go since last reboot. units are minutes
extern void EEPROMStorage_setRebootInterval (
    const uint16_t rebootMinutes);
extern uint16_t EEPROMStorage_rebootInterval (void);

extern void EEPROMStorage_setPIN (
    const CharStringSpan_t *PIN);
extern void EEPROMStorage_getPIN (
    CharString_t *PIN);

// internal temperature sensor calibration offset
extern void EEPROMStorage_setTempCalOffset (
    const int16_t offset);
extern int16_t EEPROMStorage_tempCalOffset (void);

// watchdog timer calibration. unts are 1%
extern void EEPROMStorage_setWatchdogTimerCal (
    const uint8_t wdtCal);
extern uint8_t EEPROMStorage_watchdogTimerCal (void);

// battery voltage calibration. unts are 1%
extern void EEPROMStorage_setBatteryVoltageCal (
    const uint8_t batCal);
extern uint8_t EEPROMStorage_batteryVoltageCal (void);

// water level monitor task timeout. units are seconds
extern void EEPROMStorage_setMonitorTaskTimeout (
    const uint16_t wlmTimeout);
extern uint16_t EEPROMStorage_monitorTaskTimeout (void);

// distance from the water level sensor transducer to the
// bottom of the tank when it is empty. units are cm
extern void EEPROMStorage_setWaterTankEmptyDistance (
    const uint16_t value);
extern uint16_t EEPROMStorage_waterTankEmptyDistance (void);
// distance from the water level sensor transducer to the
// surface of the water when the tank is full. units are cm
extern void EEPROMStorage_setWaterTankFullDistance (
    const uint16_t value);
extern uint16_t EEPROMStorage_waterTankFullDistance (void);

// units are percent (tank percent full)
extern void EEPROMStorage_setWaterLowNotificationLevel (
    const uint8_t level);
extern uint8_t EEPROMStorage_waterLowNotificationLevel (void);
extern void EEPROMStorage_setWaterHighNotificationLevel (
    const uint8_t level);
extern uint8_t EEPROMStorage_waterHighNotificationLevel (void);

extern void EEPROMStorage_setNotification (
    const bool onOff);
extern bool EEPROMStorage_notificationEnabled (void);

extern void EEPROMStorage_setTimeoutState (
    const uint8_t state);
extern uint8_t EEPROMStorage_timeoutState (void);

// NOTE: functions that return strings APPEND to the given string
extern void EEPROMStorage_setAPN (
    const CharStringSpan_t *APN);
extern void EEPROMStorage_getAPN (
    CharString_t *APN);
extern void EEPROMStorage_setCipqsend (
    const uint8_t qsend);
extern uint8_t EEPROMStorage_cipqsend (void);

extern void EEPROMStorage_setSampleInterval (
    const uint16_t updateInterval); // in seconds
extern uint16_t EEPROMStorage_sampleInterval (void);
extern void EEPROMStorage_setLoggingUpdateInterval (
    const uint16_t updateInterval); // in seconds
extern uint16_t EEPROMStorage_LoggingUpdateInterval (void);

//
// Storage for ThingSpeak support.
//
extern void EEPROMStorage_setThingspeak (
    const bool enabled);
extern bool EEPROMStorage_thingspeakEnabled (void);
extern void EEPROMStorage_setThingspeakHostAddress (
    const CharStringSpan_t *address);
extern void EEPROMStorage_getThingspeakHostAddress (
    CharString_t *address);
extern void EEPROMStorage_setThingspeakHostPort (
    const uint16_t port);
extern uint16_t EEPROMStorage_thingspeakHostPort (void); 
// writekey should be < 20 chars
extern void EEPROMStorage_setThingspeakWriteKey (
    const CharStringSpan_t *writekey);
extern void EEPROMStorage_getThingspeakWriteKey (
    CharString_t *writekey);

// sample time in seconds
extern void EEPROMStorage_setFilterSampleTime (
    const uint8_t sampleTime);
extern uint8_t EEPROMStorage_filterSampleTime (void);
// number of samples to include in filter calculation
extern void EEPROMStorage_setFilterSamples (
    const uint8_t samples);
extern uint8_t EEPROMStorage_filterSamples (void);
// sample variance is in units of cm H2O
extern void EEPROMStorage_setFilterVariance (
    const uint16_t variance);
extern uint16_t EEPROMStorage_filterVariance (void);

extern void EEPROMStorage_setIPConsoleEnabled (
    const bool enabled);
extern bool EEPROMStorage_ipConsoleEnabled (void);
extern void EEPROMStorage_setIPConsoleServerAddress (
    const CharStringSpan_t* server);
extern void EEPROMStorage_getIPConsoleServerAddress (
    CharString_t *server);
extern void EEPROMStorage_setIPConsoleServerPort (
    const uint16_t port);
extern uint16_t EEPROMStorage_ipConsoleServerPort (void); 

#endif		// EEPROMSTORAGE