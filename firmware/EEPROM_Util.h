//
// EEPROM access
//
#ifndef EEPROM_UTIL_H
#define EEPROM_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/eeprom.h>
#include "CharStringSpan.h"

extern void EEPROM_write (
    uint8_t* uiAddress,
    const uint8_t ucData);

extern uint8_t EEPROM_read (
    const uint8_t* uiAddress);

extern void EEPROM_writeString (
    char* uiAddress,
    const int maxLength,
    const CharStringSpan_t *string);

// Appends to given string
extern void EEPROM_readString (
    const char* uiAddress,
    CharString_t *string);

extern void EEPROM_writeWord (
    uint16_t* uiAddress,
    const uint16_t word);

extern uint16_t EEPROM_readWord (
    const uint16_t* uiAddress);

extern void EEPROM_writeLong (
    uint32_t* uiAddress,
    const uint32_t longword);

extern uint32_t EEPROM_readLong (
    const uint32_t* uiAddress);


#endif  // EEPROM_UTIL_H
