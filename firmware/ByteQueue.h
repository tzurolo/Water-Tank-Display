//
// Byte Queue
//
//  What it does:
//    Provides a fixed-capacity variable-length queue that can be
//    used to transfer data between interrupt service routines and
//    the main thread (i.e. it's protected against interrupts during
//    all operations). Supports the usual push to tail and pop from
//    head operations.
//
//  How to use it:
//    Define a queue like this:
//       ByteQueue_define(300, FromADH8066_Buffer, static)
//    which defines static variable FromADH8066_Buffer with a capacity of
//    300 bytes.
//    Then you can use the other functions to push and pop bytes.
//

#ifndef BYTEQUEUE_LOADED
#define BYTEQUEUE_LOADED

#include <stdbool.h>
#include "inttypes.h"
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

// enable/disable watermarking length of byte queues
#define BYTEQUEUE_HIGHWATERMARK_ENABLED 0

typedef uint8_t ByteQueueElement;

typedef struct {
    uint16_t head;
    uint16_t tail;
    uint16_t length;
    uint16_t capacity;
#if BYTEQUEUE_HIGHWATERMARK_ENABLED
    uint16_t highwater;
#endif
    ByteQueueElement *bytes;
    } ByteQueue_t;

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
#define ByteQueue_define(capacity, queueName, storage) \
    storage ByteQueueElement queueName##_buf[capacity] = {0}; \
    storage ByteQueue_t queueName = {0, 0, 0, capacity, 0, queueName##_buf};
#else
#define ByteQueue_define(capacity, queueName, storage) \
    storage ByteQueueElement queueName##_buf[capacity] = {0}; \
    storage ByteQueue_t queueName = {0, 0, 0, capacity, queueName##_buf};
#endif

extern void ByteQueue_clear (
    ByteQueue_t *q);

// returns the current length of the queue
inline uint16_t ByteQueue_length (
    const ByteQueue_t *q)
    {
    char SREGSave;
    SREGSave = SREG;
    cli();

    const uint16_t len = q->length;

    SREG = SREGSave;

    return len;
    }

// returns the length available in the queue
inline uint16_t ByteQueue_spaceRemaining (
    const ByteQueue_t *q)
    {
    char SREGSave;
    SREGSave = SREG;
    cli();

    const uint16_t remaining = q->capacity - q->length;

    SREG = SREGSave;

    return remaining;
    }

// returns true if the queue is currently empty
inline bool ByteQueue_is_empty (
   const ByteQueue_t *q)
   {
    char SREGSave;
    SREGSave = SREG;
    cli();

    const bool empty = q->length == 0;

    SREG = SREGSave;

    return empty;
    }

// returns true if the queue is currently full
inline bool ByteQueue_is_full (
   const ByteQueue_t *q)
   {
    char SREGSave;
    SREGSave = SREG;
    cli();

    const bool full = q->length == q->capacity;

    SREG = SREGSave;

    return full;
    }

// assumes the queue is not empty
inline ByteQueueElement ByteQueue_head (
   const ByteQueue_t *q)
{
    char SREGSave;
    SREGSave = SREG;
    cli();

    const ByteQueueElement h = q->bytes[q->head];

    SREG = SREGSave;

    return h;
}

// pushes a byte onto the tail of the queue, if it's not full. returns
// true if successful
extern bool ByteQueue_push (
   const ByteQueueElement byte,
   ByteQueue_t *q);

// pops a byte from the head of the queue, expects it's not empty
extern ByteQueueElement ByteQueue_pop (
   ByteQueue_t *q);

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
// returns the high watermark of the queue
inline uint16_t ByteQueue_highwater (
    const ByteQueue_t *q)
{
    char SREGSave;
    SREGSave = SREG;
    cli();

    const uint16_t len = q->highwater;

    SREG = SREGSave;

    return len;
}

extern void ByteQueue_reportHighwater (
   PGM_P queueName,
   ByteQueue_t *q);
#endif

#endif   // BYTEQUEUE_LOADED
