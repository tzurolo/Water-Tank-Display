//
// I/O Port Bitfield
//

#include "IOPortBitfield.h"

#include <avr/io.h>

void IOPortBitfield_init (
    const IOPortBitfield_PortSelection port,
    const uint8_t lsbPin,
    const uint8_t width,
    const bool output,
    IOPortBitfield_t *portBitfield)
{
    portBitfield->port = port;

    // compute bit mask for field
    const uint8_t pinMask = (width >= 8)
        ? 0xFF
        : ((1 << width) - 1) << lsbPin;

    portBitfield->lsbPin = lsbPin;
    portBitfield->pinMask = pinMask;
    portBitfield->isOutput = output;

    if (output) {
        switch (port) {
            case ps_a :
                // DDRA |= pinMask;
                break;
            case ps_b :
                DDRB |= pinMask;
                break;
            case ps_c :
                DDRC |= pinMask;
                break;
            case ps_d :
                DDRD |= pinMask;
                break;
        }
    } else {
        switch (port) {
            case ps_a :
                // DDRA &= ~pinMask;
                break;
            case ps_b :
                DDRB &= ~pinMask;
                break;
            case ps_c :
                DDRC &= ~pinMask;
                break;
            case ps_d :
                DDRD &= ~pinMask;
                break;
        }
    }
}

void IOPortBitfield_write (
    const uint8_t value,
    IOPortBitfield_t *portBitfield)
{
    uint8_t preparedValue;
    IOPortBitfield_prepareValueForWrite(value, portBitfield, &preparedValue);
    IOPortBitfield_writePreparedValue(preparedValue, portBitfield);
}

void IOPortBitfield_prepareValueForWrite (
    const uint8_t value,
    const IOPortBitfield_t *portBitfield,
    uint8_t *preparedValue)
{
    uint8_t prepValue = value;

    if (portBitfield->lsbPin > 0) {
        prepValue <<= portBitfield->lsbPin;
    }

    prepValue &= portBitfield->pinMask;

    *preparedValue = prepValue;
}

void IOPortBitfield_writePreparedValue (
    const uint8_t preparedValue,
    IOPortBitfield_t *portBitfield)
{
    switch (portBitfield->port) {
        case ps_a :
            //PORTA = (PORTA & ~portBitfield->pinMask) | preparedValue;
            break;
        case ps_b :
            PORTB = (PORTB & ~portBitfield->pinMask) | preparedValue;
            break;
        case ps_c :
            PORTC = (PORTC & ~portBitfield->pinMask) | preparedValue;
            break;
        case ps_d :
            PORTD = (PORTD & ~portBitfield->pinMask) | preparedValue;
            break;
    }
}

void IOPortBitfield_set (
    IOPortBitfield_t *portBitfield)
{
    switch (portBitfield->port) {
        case ps_a :
            //PORTA |= portBitfield->pinMask;
            break;
        case ps_b :
            PORTB |= portBitfield->pinMask;
            break;
        case ps_c :
            PORTC |= portBitfield->pinMask;
            break;
        case ps_d :
            PORTD |= portBitfield->pinMask;
            break;
    }
}

void IOPortBitfield_clear (
    IOPortBitfield_t *portBitfield)
{
    switch (portBitfield->port) {
        case ps_a :
            //PORTA &= ~portBitfield->pinMask;
            break;
        case ps_b :
            PORTB &= ~portBitfield->pinMask;
            break;
        case ps_c :
            PORTC &= ~portBitfield->pinMask;
            break;
        case ps_d :
            PORTD &= ~portBitfield->pinMask;
            break;
    }
}

uint8_t IOPortBitfield_read (
    const IOPortBitfield_t *portBitfield)
{
    uint8_t value = 0;

    if (portBitfield->isOutput) {
        // read back output bits
        switch (portBitfield->port) {
            case ps_a :
                //value = (PORTA & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
            case ps_b :
                value = (PORTB & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
            case ps_c :
                value = (PORTC & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
            case ps_d :
                value = (PORTD & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
        }
    } else {
        switch (portBitfield->port) {
            case ps_a :
                //value = (PINA & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
            case ps_b :
                value = (PINB & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
            case ps_c :
                value = (PINC & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
            case ps_d :
                value = (PIND & portBitfield->pinMask) >> portBitfield->lsbPin;
                break;
        }
    }

    return value;
}

bool IOPortBitfield_readAsBool (
    const IOPortBitfield_t *portBitfield)
{
    bool boolValue = false;

    if (portBitfield->isOutput) {
        // read back output bit
        switch (portBitfield->port) {
            case ps_a :
                //value = ((PORTA & portBitfield->pinMask) != 0);
                break;
            case ps_b :
                boolValue = ((PORTB & portBitfield->pinMask) != 0);
                break;
            case ps_c :
                boolValue = ((PORTC & portBitfield->pinMask) != 0);
                break;
            case ps_d :
                boolValue = ((PORTD & portBitfield->pinMask) != 0);
                break;
        }
    } else {
        switch (portBitfield->port) {
            case ps_a :
                //value = ((PINA & portBitfield->pinMask) != 0);
                break;
            case ps_b :
                boolValue = ((PINB & portBitfield->pinMask) != 0);
                break;
            case ps_c :
                boolValue = ((PINC & portBitfield->pinMask) != 0);
                break;
            case ps_d :
                boolValue = ((PIND & portBitfield->pinMask) != 0);
                break;
        }
    }

    return boolValue;
}
