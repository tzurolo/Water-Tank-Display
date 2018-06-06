//
//  SystemTime
//
//  Counts seconds since last reset
//  Resets the watchdog timer
//
//  Uses AtMega32u4 Timer/Counter 3
//
#ifndef SYSTEMTIME_H
#define SYSTEMTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "CharString.h"

#define SYSTEMTIME_TICKS_PER_SECOND 4800

// how the last reboot occurred
typedef enum SystemTime_LastRebootBy_enum {
    lrb_software,   // reboot initiated by software (e.g. reboot command)
    lrb_hardware    // reboot initiated by hardware (e.g. reset line pulled low)
} SystemTime_LastRebootBy;

// absolute time from beginning of GPS epoch
typedef struct SystemTime_t_struct {
    uint32_t seconds;   // seconds
    uint8_t hundredths; // 1/100 second
} SystemTime_t;

// prototype for functions that clients supply to
// get notification when a tick occurs
typedef void (*SystemTime_TickNotification)(void);

extern void SystemTime_Initialize (void);

// up to 2 clients can register
extern void SystemTime_registerForTickNotification (
    SystemTime_TickNotification notificationFcn);

// time since reset in 1/100 second
extern void SystemTime_getCurrentTime (
    SystemTime_t *curTime);

extern uint32_t SystemTime_uptime (void);
extern SystemTime_LastRebootBy SystemTime_LastReboot (void);

// initializes futureTime to the current time plus
// the given number of 1/100 seconds
extern void SystemTime_futureTime (
    const int hundredthsFromNow,
    SystemTime_t* futureTime);

// returns true if the current time is >= the given time
extern bool SystemTime_timeHasArrived (
    const SystemTime_t* futureTime);

// this function is used to resynchronize system time to
// server time, but not immediately. This function stores
// an offset from the given time to the current system time.
// The adjustment takes effect when you call
// SystemTime_applyTimeAdjustment()
extern void SystemTime_setTimeAdjustment (
    const uint32_t *newTime);
extern void SystemTime_applyTimeAdjustment (void);

// sleep for up to 18 hours
extern void SystemTime_sleepFor (
    const uint16_t seconds);

extern void SystemTime_commenceShutdown (void);
extern bool SystemTime_shuttingDown (void);

extern void SystemTime_task (void);

inline void SystemTime_copy (
    const SystemTime_t *from,
    SystemTime_t *to)
{
    to->seconds = from->seconds;
    to->hundredths = from->hundredths;
}

extern void SystemTime_getTaskTickRange (
    uint8_t *minTicks,
    uint8_t *maxTicks);
extern void SystemTime_resetTaskTickRange (void);

// returns t1.seconds - t2.seconds
inline int32_t SystemTime_diffSec (
    const SystemTime_t *t1,
    const SystemTime_t *t2)
{
    return t1->seconds - t2->seconds;
}

extern uint8_t SystemTime_dayOfWeek (
    const SystemTime_t *time);
extern uint8_t SystemTime_hours (
    const SystemTime_t *time);
extern uint8_t SystemTime_minutes (
    const SystemTime_t *time);
extern uint8_t SystemTime_seconds (
    const SystemTime_t *time);

// writes given time as D:HH:MM:SS
extern void SystemTime_appendToString (
    const SystemTime_t *time,
    CharString_t* timeString);

#endif  // SYSTEMTIME_H
