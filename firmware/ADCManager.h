//
//  Analog to Digital Converter Manager
//
//  Used to reserve the ADC and do conversions
//
//  Platform: AtMega32u4
//
#ifndef ADCMANAGER_H
#define ADCMANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

// ADC voltage reference selection (use one only)
#define ADC_VOLTAGE_REF_AREF            0
#define ADC_VOLTAGE_REF_AVCC            1
#define ADC_VOLTAGE_REF_INTERNAL_2V56   3

// ADC single-ended input channels (use one only)
#define ADC_SINGLE_ENDED_INPUT_ADC0   0   // PF0
#define ADC_SINGLE_ENDED_INPUT_ADC1   1   // PF1
#define ADC_SINGLE_ENDED_INPUT_ADC4   4   // PF4
#define ADC_SINGLE_ENDED_INPUT_ADC5   5   // PF5
#define ADC_SINGLE_ENDED_INPUT_ADC6   6   // PF6
#define ADC_SINGLE_ENDED_INPUT_ADC7   7   // PF7
#define ADC_SINGLE_ENDED_INPUT_1V1   30   // I Ref
#define ADC_SINGLE_ENDED_INPUT_0V    31   // AGND
#define ADC_SINGLE_ENDED_INPUT_ADC8  32
#define ADC_SINGLE_ENDED_INPUT_ADC9  33
#define ADC_SINGLE_ENDED_INPUT_ADC10 34
#define ADC_SINGLE_ENDED_INPUT_ADC11 35
#define ADC_SINGLE_ENDED_INPUT_ADC12 36
#define ADC_SINGLE_ENDED_INPUT_ADC13 37
#define ADC_SINGLE_ENDED_INPUT_TEMP  39   // Temperature sensor

typedef struct ADCManager_ChannelDesc_struct {
    uint8_t admux;
    bool mux5Set;
} ADCManager_ChannelDesc;

// called once at power-up
extern void ADCManager_Initialize (void);

// creates a channel descriptor for use in ADCManager_StartConversion
extern void ADCManager_setupChannel (
    const uint8_t channel,  // one of ADC_SINGLE_ENDED_INPUT_xxx
    const uint8_t adcRef,   // one of ADC_VOLTAGE_REF_xxx
    const bool leftAdjustResult,
    ADCManager_ChannelDesc *channelDesc);

// schedules and reads the requested channel
// called in each iteration of the mainloop
extern void ADCManager_task (void);

// returns true and starts a conversion if the ADC is
// available (not in use by another caller).
// channelDesc must be set up by ADCManager_setupChannel
extern bool ADCManager_StartConversion (
    const ADCManager_ChannelDesc *channelDesc);

// returns true when the conversion is complete and
// returns the analog value. only passes the value
// to the caller the first time it returns true
extern bool ADCManager_ConversionIsComplete (
    uint16_t* analogValue);

#endif  // ADCMANAGER_H
