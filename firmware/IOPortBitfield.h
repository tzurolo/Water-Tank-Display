//
// I/O Port Bitfield
//
//  What it does:
//    Provides a way to read and write bit fields in an I/O port,
//    where the port and bits are specified at run-time.
//
//  How to use it:
//    Declare and initialize a bit field using IOPortBitfield_init(),
//    where you specify the port, lsb, and width. Then you can read
//    and write values.
//
//    For more efficient writing you can create a 'prepared value'
//    using IOPortBitfield_prepareValueForWrite() and
//    IOPortBitfield_writePreparedValue. This does the shifting and
//    masking of the value once instead of upon every write.
//
#ifndef IOPORTBITFIELDS_H
#define IOPORTBITFIELDS_H

#include <stdint.h>
#include <stdbool.h>

// indicates a particular I/O port
typedef enum {
    ps_a,
    ps_b,
    ps_c,
    ps_d
} IOPortBitfield_PortSelection;

typedef struct IOPortBitfield_struct {
    IOPortBitfield_PortSelection port;
    uint8_t lsbPin;
    uint8_t pinMask;
    bool isOutput;
} IOPortBitfield_t;

extern void IOPortBitfield_init (
    const IOPortBitfield_PortSelection port,
    const uint8_t lsbPin,
    const uint8_t width,
    const bool output,  // true for output, false for input
    IOPortBitfield_t *portBitfield);

// for a more efficient way to write to a bit
// field port consider using prepareValueForWrite
// and writePreparedValue
extern void IOPortBitfield_write (
    const uint8_t value,
    IOPortBitfield_t *portBitfield);

// prepares a value for writePreparedValue
// by pre-shifting and masking the given value
extern void IOPortBitfield_prepareValueForWrite (
    const uint8_t value,
    const IOPortBitfield_t *portBitfield,
    uint8_t *preparedValue);

// writes a value prepared by prepareValueForWrite
extern void IOPortBitfield_writePreparedValue (
    const uint8_t preparedValue,
    IOPortBitfield_t *portBitfield);

// sets all bits of the field to 1
extern void IOPortBitfield_set (
    IOPortBitfield_t *portBitfield);

// sets all bits of the field to 0
extern void IOPortBitfield_clear (
    IOPortBitfield_t *portBitfield);

extern uint8_t IOPortBitfield_read (
    const IOPortBitfield_t *portBitfield);

extern bool IOPortBitfield_readAsBool (
    const IOPortBitfield_t *portBitfield);

#endif      // #ifndef IOPORTBITFIELDS_H
