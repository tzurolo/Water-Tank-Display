//
//  Analog to Digital Converter Manager
//
//  Platform: AtMega32u4
//
// Need way to 'finalize', that is, release resources to reduce power
//

#include "ADCManager.h"

//#include "SystemTime.h"
#include <stdlib.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

// Analog to Digital converter - AtMega328P
#define ADC_MUX_MASK 0x1F
// ADC clock prescaler
#define ADC_PRESCALER_2     0
#define ADC_PRESCALER_2a    1
#define ADC_PRESCALER_4     2
#define ADC_PRESCALER_8     3
#define ADC_PRESCALER_16    4
#define ADC_PRESCALER_32    5
#define ADC_PRESCALER_64    6
#define ADC_PRESCALER_128   7
// ADC Control and Status
#define ADC_ENABLE              (1 << ADEN)
#define ADC_START               (1 << ADSC)
#define ADC_AUTO_TRIGGER_ENABLE (1 << ADATE)
#define ADC_INTERRUPT_FLAG      (1 << ADIF)
#define ADC_INTERRUPT_ENABLE    (1 << ADIE)
#define ADC_LEFT_ADJUST_RESULT  (1 << ADLAR)
#define ADC_RIGHT_ADJUST_RESULT (0)

// controller states
typedef enum ADCManagerState_enum {
    adcms_idle,
    adcms_waitingForADCFirstSample,
    adcms_waitingForADCSecondSample,
    adcms_conversionComplete
} ADCManagerState;
// state variables
static ADCManagerState adcmsState = adcms_idle;
static ADCManager_ChannelDesc currentChannelDesc;

static void ADC_Init (
    const uint8_t prescale)
{
    ADCSRA = (ADCSRA & 0xF8) | prescale;
    ADCSRA |= ADC_ENABLE;
}

// admux consists of voltage reference selection and  input channel selection
static void ADC_StartConversion ()
{
    ADMUX = currentChannelDesc.admux;
    if (currentChannelDesc.mux5Set) {
        ADCSRB |= (1 << MUX5);
    } else {
        ADCSRB &= ~(1 << MUX5);
    }
   
    ADCSRA |= ADC_START;
}

static bool ADC_ConversionIsFinished (void)
{
    return ((ADCSRA & ADC_INTERRUPT_FLAG) != 0);
}

// right-adjusted result
static uint16_t ADC_Result (void)
{
    ADCSRA |= ADC_INTERRUPT_FLAG;
    if ((ADMUX & (1 << ADLAR)) != 0) {
        return (uint16_t)ADCH;
    } else {
        return ADC;
    }
}

void ADCManager_Initialize (void)
{
    // set ADC for single-conversion, clock/16 prescaler
    ADC_Init(ADC_PRESCALER_16);

    adcmsState = adcms_idle;
}

void ADCManager_setupChannel (
    const uint8_t channel,
    const uint8_t adcRef,
    const bool leftAdjustResult,
    ADCManager_ChannelDesc *channelDesc)
{
    channelDesc->admux = 
        (channel & 0x1F) |
        (leftAdjustResult ? ADC_LEFT_ADJUST_RESULT : 0) |
        ((adcRef & 3) << REFS0);
    channelDesc->mux5Set = (channel & 0x20) != 0;

    // disable digital input on corresponding pin
    if (channel <= ADC_SINGLE_ENDED_INPUT_ADC7) {
        DIDR0 |= (1 << channel);
    } else if ((channel >= ADC_SINGLE_ENDED_INPUT_ADC8) && 
               (channel <= ADC_SINGLE_ENDED_INPUT_ADC13)) {
        DIDR1 |= (1 << (channel - ADC_SINGLE_ENDED_INPUT_ADC8));
    }
}

void ADCManager_task (void)
{
    switch (adcmsState) {
        case adcms_idle : {
            }
            break;
        case adcms_waitingForADCFirstSample : {
            if (ADC_ConversionIsFinished()) {
                // start a second conversion (allows S/H time to settle)
                ADC_StartConversion();
                adcmsState = adcms_waitingForADCSecondSample;
            }
            }
            break;
        case adcms_waitingForADCSecondSample : {
            if (ADC_ConversionIsFinished()) {
                adcmsState = adcms_conversionComplete;
            }
            }
            break;
        case adcms_conversionComplete : {
            }
            break;
        default :
            // unknown state. reboot
            // SystemTime_commenceShutdown();
            break;
    }
}

bool ADCManager_StartConversion (
    const ADCManager_ChannelDesc *channelDesc)
{
    bool started = false;

    if (adcmsState == adcms_idle) {
        // nobody currently using ADC
        currentChannelDesc = *channelDesc;
        ADC_StartConversion();
        adcmsState = adcms_waitingForADCFirstSample;
        started = true;
    }

    return started;
}

bool ADCManager_ConversionIsComplete (
    uint16_t* analogValue)
{
    bool completed = false;
    
    if (adcmsState == adcms_conversionComplete) {
        completed = true;
        *analogValue = ADC_Result();
        adcmsState = adcms_idle;
    }

    return completed;
}

