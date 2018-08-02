//
//  Pump Monitor
//
//  Monitors power to the pump, so we can report when it goes on and off
//
//  How it works:
//      The mains monitor is simply an input pin connected to a voltage
//      divider powered by the USB pin on the Feather Fona. When the USB pin
//      has power it will be at 5V and the input pin should be at a voltage
//      higher than when the USB pin is not powered (something around the
//      battery voltage).
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
#include "ADCManager.h"

#define PUMP_SAMPLE_INTERVAL 5
#define MAINS_SAMPLE_INTERVAL 50

// ADC result counts between mains off and mains on
// volage divider is 2.6K and 8K, ratio is 0.245. The ADC
// voltage reference is set to 2.56 volts, so 5V on the USB pin
// should give a result count of about 490. 4.2V on the USB pin
// (max battery voltage) should give a result count of about 412.
// we pick a value in the middle of those two result counts
#define MAINS_ON_ADC_VALUE 451

#define OPTOISOLATOR_DDR        DDRF
#define OPTOISOLATOR_INPORT     PINF
#define OPTOISOLATOR_OUTPORT    PORTF
#define OPTOISOLATOR_PIN        PF4

typedef enum mainsTaskState_enum {
    mts_idle,
    mts_waitingForADCStart,
    mts_waitingForADCCompletion
} mainsTaskState;

static bool pumpOn;
static bool mainsOn;
static uint16_t numSamples;
static uint16_t numAssertedSamples;
static SystemTime_t pumpSampleTimer;
static SystemTime_t mainsSampleTimer;
static ADCManager_ChannelDesc mainsADCChannelDesc;
static PowerMonitor_Notification notification = NULL;
static mainsTaskState mtState;
static bool mainsOn;

void PowerMonitor_Initialize (void)
{
    pumpOn = false;
    mainsOn = false;
    numSamples = 0;
    numAssertedSamples = 0;
    notification = NULL;

    // set up optoisolator pin as input, turn on pull-up
    OPTOISOLATOR_DDR &= (~(1 << OPTOISOLATOR_PIN));
    OPTOISOLATOR_OUTPORT |= (1 << OPTOISOLATOR_PIN);

    // set up mains monitor ADC channel
    ADCManager_setupChannel(
        ADC_SINGLE_ENDED_INPUT_ADC5, ADC_VOLTAGE_REF_INTERNAL_2V56, false,
        &mainsADCChannelDesc);

    // start sample interval timers
    SystemTime_futureTime(PUMP_SAMPLE_INTERVAL, &pumpSampleTimer);
    SystemTime_futureTime(MAINS_SAMPLE_INTERVAL, &mainsSampleTimer);

    mtState = mts_idle;
    mainsOn = false;
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
    return mainsOn;
}

static void pumpTask (void)
{
    if (SystemTime_timeHasArrived(&pumpSampleTimer)) {
        SystemTime_futureTime(PUMP_SAMPLE_INTERVAL, &pumpSampleTimer);

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

static void mainsTask (void)
{
    switch (mtState) {
        case mts_idle :
            if (SystemTime_timeHasArrived(&mainsSampleTimer)) {
                SystemTime_futureTime(MAINS_SAMPLE_INTERVAL, &mainsSampleTimer);
                mtState = mts_waitingForADCStart;
            }
            break;
        case mts_waitingForADCStart :
            if (ADCManager_StartConversion(&mainsADCChannelDesc)) {
                // successfully started conversion
                mtState = mts_waitingForADCCompletion;
            }
            break;    
        case mts_waitingForADCCompletion : {
            uint16_t voltage;
            if (ADCManager_ConversionIsComplete(&voltage)) {
                mainsOn = (voltage > MAINS_ON_ADC_VALUE);

                mtState = mts_idle;
            }
            }
            break;
    }
}

void PowerMonitor_task (void)
{
    pumpTask();
    mainsTask();
}

