//
// Byte Queue
//

#include "ByteQueue.h"

// these are needed only for reporting highwater mark
#include "CharString.h"
#include "Console.h"
#include "StringUtils.h"

void ByteQueue_clear (
    ByteQueue_t *q)
    {
    char SREGSave;
    SREGSave = SREG;
    cli();

    q->head = 0;
    q->tail = 0;
    q->length = 0;

    SREG = SREGSave;
    }

bool ByteQueue_push (
    const ByteQueueElement byte,
    ByteQueue_t *q)
    {
    bool push_successful = false;

    char SREGSave;
    SREGSave = SREG;
    cli();

    if (q->length < q->capacity)
        {  // the queue is not full
        // put the byte in the queue
        q->bytes[q->tail] = byte;

        // advance the tail pointer
        if (q->tail < (q->capacity - 1))
            ++q->tail;
        else  // wrap around
            q->tail = 0;

        // increment length
        ++q->length;
#if BYTEQUEUE_HIGHWATERMARK_ENABLED
        if (q->length > q->highwater) {
            q->highwater = q->length;
        }
#endif

        push_successful = true;
        }

    SREG = SREGSave;

    return push_successful;
    }

ByteQueueElement ByteQueue_pop (
    ByteQueue_t *q)
{
    ByteQueueElement byte = 0;

    char SREGSave;
    SREGSave = SREG;
    cli();

    if (q->length > 0) {  // the queue is not empty
        // get the byte from the queue
        byte = q->bytes[q->head];

        // advance the head pointer
        if (q->head < (q->capacity - 1))
            ++q->head;
        else  // wrap around
            q->head = 0;

        // decrement length
        --q->length;
    }

    SREG = SREGSave;

    return byte;
}

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
extern void ByteQueue_reportHighwater (
   PGM_P queueName,
   ByteQueue_t *q)
{
    CharString_define(20, msg);
    CharString_copyP(queueName, &msg);
    CharString_appendC(':', &msg);
    StringUtils_appendDecimal(q->highwater, 1, 0, &msg);
    CharString_appendC('/', &msg);
    StringUtils_appendDecimal(q->capacity, 1, 0, &msg);
    Console_printCS(&msg);
}
#endif