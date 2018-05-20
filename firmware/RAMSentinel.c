//
//  RAM Sentinel
//
//  How it works:
//      Defines a byte of storage and initializes it to a particular bit pattern.
//      when RAMSentinel_checkIntegrity() is called it checks to see if the storage
//      still has the pattern. If the stack overflowed it will likely have a different
//      value.
//

#include "RAMSentinel.h"

#include "Console.h"
#include "StringUtils.h"
#include "avr/io.h"

#define SENTINEL_VALUE 0xAA

static uint8_t sentinel;

void RAMSentinel_Initialize (void)
{
    sentinel = SENTINEL_VALUE;
}

bool RAMSentinel_sentinelIntact (void)
{
    if (sentinel == SENTINEL_VALUE) {
        return true;
    } else {
        // reset sentinel
        sentinel = SENTINEL_VALUE;
        return false;
    }
}

void RAMSentinel_printStackPtr (void)
{
    CharString_define(8, msg);
    CharString_clear(&msg);
    CharString_appendC('<', &msg);
    StringUtils_appendDecimal(SP, 4, 0, &msg);
    CharString_appendC('>', &msg);
    Console_printCS(&msg);
}
