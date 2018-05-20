//
//  Queue (uin16_t)
//

#ifndef MessageIDQueue_H
#define MessageIDQueue_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

#define MessageIDQueue_Capacity 8

typedef uint16_t MessageIDQueue_ValueType;

typedef struct {
    uint8_t head;
    uint8_t tail;
    uint8_t length;
    MessageIDQueue_ValueType buffer[MessageIDQueue_Capacity];
} MessageIDQueue_type;

extern void MessageIDQueue_init (
    MessageIDQueue_type* queue);

extern bool MessageIDQueue_isEmpty (
    MessageIDQueue_type* queue);

extern bool MessageIDQueue_isFull (
    MessageIDQueue_type* queue);

extern void MessageIDQueue_insert (
    const MessageIDQueue_ValueType value,
    MessageIDQueue_type* queue);

extern MessageIDQueue_ValueType MessageIDQueue_remove (
    MessageIDQueue_type* queue);

#endif  // MessageIDQueue_H
