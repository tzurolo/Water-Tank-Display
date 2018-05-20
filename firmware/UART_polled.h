//
// UART interface
//
//  What it does:
//    This interface is designed to work seamlessly with other tasks while
//    providing asynchronous communications. It does not use interrupts but
//    expects to be called periodically to transmit and receive. On each
//    call to UART_update() it will do a minimal amount of work (processing
//    one character) to leave enough time for other tasks to do their work.
//    UART_update() must be called at a rate faster than the expected rate
//    receiving characters.
//    This interface has a 400-byte transmit queue and an 80 byte receive
//    queue. The writing functions return false if there is not enough room
//    in the transmit queue. If the receive queue fills up characters are
//    lost.
//
//  How to use it:
//    Upon powerup call UART_init() once.
//    Each iteration of the program's mainloop call UART_update().
//    Any time during the mainloop you can call the other functions to
//    read and write data.
//
//  Hardware resouces used:
//    TxD and RxD pins
//

#ifndef UART_LOADED
#define UART_LOADED

#include <stdbool.h>
#include <stdint.h>
#include <avr/pgmspace.h>
#include "StringBuffer.h"

// uart initialization. must be called once before calling any of the other
// functions
extern void UART_init (void);

extern void UART_set_baud_rate (
   const uint16_t new_baud_rate);

// processes reading and writing of bytes. expected to be called more
// frequently than characters are expected to arrive to avoid overrun
extern void UART_update (void);

// reads one byte from the uart into byte param, if present. returns true
// if a byte is present, false if not. 
extern bool UART_read_byte (
   char *byte);

// writes one byte to the output queue, if it's not full. returns true if
// there was room for the byte in the queue, false if not.
extern bool UART_write_byte (
   const char byte);

// returns true if there are no bytes in the transmit queue. This function can
// be called in a loop to "wait" for all bytes to be transmitted. For example:
//       while (!UART_tx_queue_is_empty())
//          UART_update();
extern bool UART_tx_queue_is_empty (void);

// writes a null-terminated string to the output queue, if there is room for
// the whole string in the queue. returns true if there was room, and false
// if not. if not enough room none of the string is put in the queue.
extern bool UART_write_string (
   const char *string);
extern bool UART_write_stringP (
   PGM_P string);

// tries to read one byte from the UART and append it to the string buffer.
// returns true if the newline character was received
extern bool UART_read_string (
   StringBuffer *str_buf);


#endif   // UART_LOADED
