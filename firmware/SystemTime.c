//
//  System Time
//
//  Uses AtMega328P 16 bit timer/counter 1
//
#include "SystemTime.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include "StringUtils.h"
#include "Console.h"
#include "EEPROMStorage.h"

#define DEBUG_TRACE 1

static volatile uint16_t tickCounter = 0;
static volatile SystemTime_t currentTime;
static volatile uint32_t secondsSinceStartup;
static int32_t timeAdjustment;
static bool shuttingDown = false;
static SystemTime_LastRebootBy lastRebootBy;
static SystemTime_TickNotification notificationFunction;

void SystemTime_Initialize (void)
{
    tickCounter = 0;
    currentTime.seconds = EEPROMStorage_lastRebootTimeSec();
    currentTime.hundredths = 0;
    EEPROMStorage_setLastRebootTimeSec(0);
    secondsSinceStartup = 0;
    timeAdjustment = 0;
    shuttingDown = false;
    lastRebootBy = (currentTime.seconds > 1)
        ? lrb_software
        : lrb_hardware;
    notificationFunction = 0;

    // set up timer1 to fire interrupt at SYSTEMTIME_TICKS_PER_SECOND
    TCCR1B = (TCCR1B & 0xF8) | 2; // prescale by 8
    TCCR1B = (TCCR1B & 0xE7) | (1 << 3); // set CTC mode
    OCR1A = (F_CPU / 8) / SYSTEMTIME_TICKS_PER_SECOND;
    TCNT1 = 0;  // start the time counter at 0
    TIFR1 |= (1 << OCF1A);  // "clear" the timer compare flag
    TIMSK1 |= (1 << OCIE1A);// enable timer compare match interrupt
}

void SystemTime_registerForTickNotification (
    SystemTime_TickNotification notificationFcn)
{
    notificationFunction = notificationFcn;
}

void SystemTime_getCurrentTime (
    SystemTime_t *curTime)
{
    // we disable interrupts during read of secondsSinceReset because
    // it is updated in an interrupt handler
    char SREGSave;
    SREGSave = SREG;
    cli();
    curTime->seconds = currentTime.seconds;
    curTime->hundredths = currentTime.hundredths;
    SREG = SREGSave;
}

uint32_t SystemTime_uptime (void)
{
    uint32_t uptime;

    char SREGSave;
    SREGSave = SREG;
    cli();
    uptime = secondsSinceStartup;
    SREG = SREGSave;

    return uptime;
}

SystemTime_LastRebootBy SystemTime_LastReboot (void)
{
    return lastRebootBy;
}

void SystemTime_futureTime (
    const int hundredthsFromNow,
    SystemTime_t* futureTime)
{
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    SystemTime_copy(&curTime, futureTime);
    futureTime->hundredths += hundredthsFromNow % 100;
    if (futureTime->hundredths >= 100) {
        // overflow - carry
        futureTime->hundredths -= 100;
        ++futureTime->seconds;
    }
    futureTime->seconds += hundredthsFromNow / 100;
}

bool SystemTime_timeHasArrived (
    const SystemTime_t* futureTime)
{
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    return (curTime.seconds > futureTime->seconds)
        ? true
        : (curTime.seconds == futureTime->seconds)
            ? curTime.hundredths >= futureTime->hundredths
            : false;
}

void SystemTime_setTimeAdjustment (
    const uint32_t *newTime)
{
    char SREGSave;
    SREGSave = SREG;
    cli();
    timeAdjustment = *newTime - currentTime.seconds;
    SREG = SREGSave;
#if DEBUG_TRACE
    {
        CharString_define(20, msg);
        CharString_copyP(PSTR("Tadj: "), &msg);
        StringUtils_appendDecimal32(timeAdjustment, 1, 0, &msg);
        Console_printCS(&msg);
    }
#endif
}

void SystemTime_applyTimeAdjustment ()
{
    char SREGSave;

    SREGSave = SREG;
    cli();
    currentTime.seconds += timeAdjustment;
    timeAdjustment = 0;
    SREG = SREGSave;
}

void SystemTime_sleepFor (
    const uint16_t seconds)
{
    uint32_t calibratedSeconds = 
        (((uint32_t)seconds) * EEPROMStorage_watchdogTimerCal()) / 100;
    uint16_t secondsRemaining = calibratedSeconds;
    while (secondsRemaining > 0) {
        uint8_t wdtTimeout;
        uint8_t secondsThisLoop;
        if (secondsRemaining >= 8) {
            wdtTimeout = WDTO_8S;
            secondsThisLoop = 8;
        } else if (secondsRemaining >= 4) {
            wdtTimeout = WDTO_4S;
            secondsThisLoop = 4;
        } else if (secondsRemaining >= 2) {
            wdtTimeout = WDTO_2S;
            secondsThisLoop = 2;
        } else {
            wdtTimeout = WDTO_1S;
            secondsThisLoop = 1;
        }
        secondsRemaining -= secondsThisLoop;
        wdt_enable(wdtTimeout);
        WDTCSR |= (1 << WDIE);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        cli();
        sleep_enable();
        sleep_bod_disable();

        sei();
        sleep_cpu();
        sleep_disable();
        sei();
    }
    cli();
    currentTime.seconds += seconds;
    secondsSinceStartup += seconds;
    sei();
}

void SystemTime_commenceShutdown (void)
{
    if (!shuttingDown) {
        shuttingDown = true;
        // store reboot time
        SystemTime_t curTime;
        SystemTime_getCurrentTime(&curTime);
        curTime.seconds += timeAdjustment;    // apply time adjustment
        curTime.seconds += 8;  // account for watchdog time
        EEPROMStorage_setLastRebootTimeSec(curTime.seconds);

        Console_printP(PSTR("shutting down..."));
        wdt_enable(WDTO_8S);
        wdt_reset();
    }
}

bool SystemTime_shuttingDown (void)
{
    return shuttingDown;
}

void SystemTime_task (void)
{
    // reset the watchdog timer
    if (shuttingDown) {
//        LED_OUTPORT |= (1 << LED_PIN);
    } else {
        wdt_reset();

        // reboot if it's been more than the stored reboot interval plus 3 logging intervals
        // since startup
        const uint32_t uptime = SystemTime_uptime();
        const uint32_t rebootIntervalSeconds =
            (((uint32_t)EEPROMStorage_rebootInterval()) * 60) +
            (((uint32_t)EEPROMStorage_LoggingUpdateInterval()) * 3);
        if (uptime > rebootIntervalSeconds) {
            SystemTime_commenceShutdown();
        }
    }
}

uint8_t SystemTime_dayOfWeek (
    const SystemTime_t *time)
{
    return (time->seconds / 86400L) % 7;
}

uint8_t SystemTime_hours (
    const SystemTime_t *time)
{
    return (time->seconds / 3600) % 24;
}

uint8_t SystemTime_minutes (
    const SystemTime_t *time)
{
    return (time->seconds / 60) % 60;
}

uint8_t SystemTime_seconds (
    const SystemTime_t *time)
{
    return time->seconds % 60;
}

void SystemTime_appendToString (
    const SystemTime_t *time,
    CharString_t* timeString)
{
    // append day of week
    StringUtils_appendDecimal(SystemTime_dayOfWeek(time), 1, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append hours
    StringUtils_appendDecimal(SystemTime_hours(time), 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append minutes
    StringUtils_appendDecimal(SystemTime_minutes(time), 2, 0, timeString);
    CharString_appendP(PSTR(":"), timeString);

    // append seconds
    StringUtils_appendDecimal(SystemTime_seconds(time), 2, 0, timeString);
}

ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    ++tickCounter;
    if (tickCounter >= (SYSTEMTIME_TICKS_PER_SECOND / 100)) {
        tickCounter = 0;
        ++currentTime.hundredths;
        if (currentTime.hundredths >= 100) {
            currentTime.hundredths = 0;
            ++currentTime.seconds;
            ++secondsSinceStartup;
        }
    }

    if (notificationFunction != NULL) {
        notificationFunction();
    }

}

ISR(WDT_vect, ISR_BLOCK)
{
}
