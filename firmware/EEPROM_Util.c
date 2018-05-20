//
// EEPROM access
//

#include "EEPROM_Util.h"

#include <avr/io.h>

void EEPROM_write (
    uint8_t* uiAddress,
    const uint8_t ucData)
{
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE))
        ;
    /* Set up address and Data Registers */
    EEAR = (uint16_t)uiAddress;
    EEDR = ucData;
    /* Write logical one to EEMPE */
    EECR |= (1<<EEMPE);
    /* Start eeprom write by setting EEPE */
    EECR |= (1<<EEPE);
}

uint8_t EEPROM_read (
    const uint8_t* uiAddress)
{
    /* Wait for completion of previous write */
    while (EECR & (1<<EEPE))
        ;
    /* Set up address register */
    EEAR = (uint16_t)uiAddress;
    /* Start eeprom read by writing EERE */
    EECR |= (1<<EERE);
    /* Return data from Data Register */
    return EEDR;
}

void EEPROM_writeString (
    char* uiAddress,
    const int maxLength,
    const CharStringSpan_t *string)
{
    int lengthWritten = 1;  // account for null terminator
    char strChar;
    uint8_t* charAddr = (uint8_t*)uiAddress;
    CharString_Iter cp = CharStringSpan_begin(string);
    CharString_Iter end = CharStringSpan_end(string);
    while ((lengthWritten < maxLength) && (cp != end)) {
        strChar = *cp++;
        EEPROM_write(charAddr++, strChar);
        ++lengthWritten;
    }
    EEPROM_write(charAddr, 0);    // null-terminate
}

void EEPROM_readString (
    const char* uiAddress,
    CharString_t *string)
{
    int lengthRead = 0;
    char strChar;
    uint8_t* charAddr = (uint8_t*)uiAddress;
    do {
        strChar = EEPROM_read(charAddr++);
        if (strChar != 0) {
            CharString_appendC(strChar, string);
            ++lengthRead;
        }
    } while (strChar != 0);
}

void EEPROM_writeWord (
    uint16_t* uiAddress,
    const uint16_t word)
{
    uint8_t* byteAddr = (uint8_t*)uiAddress;
    EEPROM_write(byteAddr, word & 0xFF);
    EEPROM_write(byteAddr + 1, (word >> 8) & 0xFF);
}

uint16_t EEPROM_readWord (
    const uint16_t* uiAddress)
{
    uint8_t* byteAddr = (uint8_t*)uiAddress;
    uint16_t word = 0;
    word = EEPROM_read(byteAddr);
    word |= (EEPROM_read(byteAddr + 1) << 8);

    return word;
}

void EEPROM_writeLong (
    uint32_t* uiAddress,
    const uint32_t longword)
{
    uint8_t* byteAddr = (uint8_t*)uiAddress;
    uint32_t remainingValue = longword;
    for (int byteOffset = 0; byteOffset < 4; ++byteOffset) {
        EEPROM_write(byteAddr + byteOffset, remainingValue & 0xFF);
        remainingValue >>= 8;
    }
}

uint32_t EEPROM_readLong (
    const uint32_t* uiAddress)
{
    uint8_t* byteAddr = (uint8_t*)uiAddress;
    uint32_t longword = 0;
    for (int byteOffset = 3; byteOffset >= 0; --byteOffset) {
        longword <<= 8;
        longword |= EEPROM_read(byteAddr + byteOffset);
    }
    return longword;
}
