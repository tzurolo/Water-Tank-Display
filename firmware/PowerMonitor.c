//
//  Pump Monitor
//
//  Monitors power to the pump, so we can report when it goes on and off
//
//  How it works:
//      The mains monitor is simply an input pin connected to a voltage
//      divider powered by the USB pin on the Feather Fona. When that pin
//      has power it will be at 5V and the input pin should be at logic 1.
//
//      The pump monitor uses an AC optoisolator. We sample the input pin
//      every iteration and count when it is low (AC driving LEDs in
//      optoisolator). If the pump is on we expect the input pin to be
//      low at least twice during the sample period (50mS).
//
//  Pin usage:
//     PF4 - input from optoisolator
//     PF5 - input from USB pin voltage divider
//
#include "PowerMonitor.h"

#include "SystemTime.h"
#include <avr/io.h>

#define SAMPLE_INTERVAL 5

#define OPTOISOLATOR_DDR        DDRF
#define OPTOISOLATOR_INPORT     PINF
#define OPTOISOLATOR_OUTPORT    PORTF
#define OPTOISOLATOR_PIN        PF4

#define USBPOWER_DDR        DDRF
#define USBPOWER_INPORT     PINF
#define USBPOWER_OUTPORT    PORTF
#define USBPOWER_PIN        PF5

static bool pumpOn = false;
static uint16_t numSamples;
static uint16_t numAssertedSamples;
static SystemTime_t sampleTimer;
static PowerMonitor_Notification notification = NULL;

void PowerMonitor_Initialize (void)
{
    pumpOn = false;
    numSamples = 0;
    numAssertedSamples = 0;
    notification = NULL;

    // set up mains pin as input. no pullup
    USBPOWER_DDR &= (~(1 << USBPOWER_PIN));

    // set up optoisolator pin as input, turn on pull-up
    OPTOISOLATOR_DDR &= (~(1 << OPTOISOLATOR_PIN));
    OPTOISOLATOR_OUTPORT |= (1 << OPTOISOLATOR_PIN);

    // start sample interval timer
    SystemTime_futureTime(SAMPLE_INTERVAL, &sampleTimer);
}

void PowerMonitor_registerForNotification (
    PowerMonitor_Notification notificationFunction)
{
    notification = notificationFunction;
}

bool PowerMonitor_pumpOn (void)
{
    return pumpOn;
}

bool PowerMonitor_mainsOn (void)
{
    // we just read the voltage divider pin
    return ((USBPOWER_INPORT & (1 << USBPOWER_PIN)) != 0);
}

void PowerMonitor_task (void)
{
    if (SystemTime_timeHasArrived(&sampleTimer)) {
        SystemTime_futureTime(SAMPLE_INTERVAL, &sampleTimer);

        if (numSamples >= 8) {
            // check for at least 2 samples asserted
            pumpOn = (numAssertedSamples >= 2);
        }

        numSamples = 0;
        numAssertedSamples = 0;
    } else {
        ++numSamples;
        if ((OPTOISOLATOR_INPORT & (1 << OPTOISOLATOR_PIN)) == 0) {
            ++numAssertedSamples;
        }
    }
}

