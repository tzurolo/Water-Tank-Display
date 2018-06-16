//
//  Internal Temperature Monitor
//
//  Reads the AtMega32u4's internal temperature sensor through the ADC
//

#include "InternalTemperatureMonitor.h"

#include "ADCManager.h"
#include "SystemTime.h"
#include "EEPROMStorage.h"

#define SENSOR_ADC_CHANNEL ADC_SINGLE_ENDED_INPUT_TEMP

#define RESOLUTION 1000
#define NUMERATOR ((int32_t)((1 / 0.94) * (1100 / 1024) * RESOLUTION))

// sample every 10 seconds
#define SENSOR_SAMPLE_TIME 500

typedef enum {
    tms_idle,
    tms_waitingForADCStart,
    tms_waitingForADCCompletion
} TemperatureMonitor_state;

static TemperatureMonitor_state tmState = tms_idle;
static SystemTime_t sampleTimer;
static bool haveValidSample;
static uint16_t latestTempSample;
static ADCManager_ChannelDesc adcChannelDesc;

void InternalTemperatureMonitor_Initialize (void)
{
    tmState = tms_idle;
    // start sampling in 1/50 second (20ms, to let power stabilize)
    SystemTime_futureTime(2, &sampleTimer);
    haveValidSample = false;

    // set up the ADC channel for measuring battery voltage
    ADCManager_setupChannel(
        SENSOR_ADC_CHANNEL, ADC_VOLTAGE_REF_INTERNAL_2V56, false,
        &adcChannelDesc);
}

bool InternalTemperatureMonitor_haveValidSample (void)
{
    return haveValidSample;
}

int16_t InternalTemperatureMonitor_currentTemperature (void)
{
    int16_t curTempC = 0;

    if (InternalTemperatureMonitor_haveValidSample()) {
        int32_t temp = latestTempSample;
        // counts to degrees C
        const int16_t tempCalOffset = EEPROMStorage_tempCalOffset();
        curTempC = ((int16_t)(((temp - tempCalOffset) * NUMERATOR) / RESOLUTION));
    }

    return curTempC;
}

void InternalTemperatureMonitor_task (void)
{
    switch (tmState) {
        case tms_idle :
            if (SystemTime_timeHasArrived(&sampleTimer)) {
                SystemTime_futureTime(SENSOR_SAMPLE_TIME, &sampleTimer);
                tmState = tms_waitingForADCStart;
            }
            break;
        case tms_waitingForADCStart :
            if (ADCManager_StartConversion(&adcChannelDesc)) {
                // successfully started conversion
                tmState = tms_waitingForADCCompletion;
            }
            break;    
        case tms_waitingForADCCompletion : {
            uint16_t temperature;
            if (ADCManager_ConversionIsComplete(&temperature)) {
                latestTempSample = temperature;
                haveValidSample = true;

                tmState = tms_idle;
            }
            }
            break;
    }
}

