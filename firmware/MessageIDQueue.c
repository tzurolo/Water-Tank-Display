
#include "MessageIDQueue.h"

void MessageIDQueue_init (
    MessageIDQueue_type* queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->length = 0;
}

bool MessageIDQueue_isEmpty (
    MessageIDQueue_type* queue)
{
    return (queue->length == 0);
}

bool MessageIDQueue_isFull (
    MessageIDQueue_type* queue)
{
    return (queue->length == MessageIDQueue_Capacity);
}

void MessageIDQueue_insert (
    const MessageIDQueue_ValueType value,
    MessageIDQueue_type* queue)
{
    queue->buffer[queue->tail++] = value;
    ++queue->length;
    if (queue->tail == MessageIDQueue_Capacity) {
        // wrap around
        queue->tail = 0;
    }
}

MessageIDQueue_ValueType MessageIDQueue_remove (
    MessageIDQueue_type* queue)
{
    const MessageIDQueue_ValueType value =
        queue->buffer[queue->head++];
    --queue->length;
    if (queue->head == MessageIDQueue_Capacity) {
        // wrap around
        queue->head = 0;
    }

    return value;
}
