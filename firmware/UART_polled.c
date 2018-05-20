//
// UART interface
//
//  Platform:
//     AtMega32u4
//
//  How it works:
//     Two static queues are used, one for transmit and one for receive.
//


#include "UART_polled.h"

#include "ByteQueue.h"
#include <avr/io.h>
#include <string.h>

ByteQueue_define(400, tx_queue);
ByteQueue_define(80, rx_queue);

void UART_init ()
   {
   // initialize transmit and receive queues
   ByteQueue_clear(&tx_queue);
   ByteQueue_clear(&rx_queue);

   // set default baud rate
   UART_set_baud_rate(9600);

   // enable receiver and transmitter
   UCSR1B = (1<<RXEN1)|(1<<TXEN1);
   // set frame format to 8-N-1
   UCSR1C = (3<<UCSZ10);
   }

void UART_set_baud_rate (
   const uint16_t new_baud_rate)
   {
   uint16_t ubrr1 = ((F_CPU >> 4) / new_baud_rate) - 1;
   UBRR1L = (uint8_t)ubrr1;
   UBRR1H = (uint8_t)((ubrr1 >> 8) & 0x0F);
   }

void UART_update ()
{
    // check if anything has come in from the uart
    if ((UCSR1A & (1<<RXC1)) != 0) {
        ByteQueue_push(UDR1, &rx_queue);
    }

    // check if the tranmitter is ready and there's something to write 
    if (((UCSR1A & (1<<UDRE1)) != 0) && !ByteQueue_is_empty(&tx_queue)) {
        const char tx_byte = ByteQueue_pop(&tx_queue);
        UDR1 = tx_byte;
    }
}

bool UART_read_byte (
   char *byte)
   {
       bool gotByte = false;

       if (!ByteQueue_is_empty(&rx_queue)) {
            *byte = ByteQueue_pop(&rx_queue);
            gotByte = true;
       }

       return gotByte;
   }

bool UART_write_byte (
   const char byte)
   {
   return ByteQueue_push(byte, &tx_queue);
   }

bool UART_tx_queue_is_empty ()
   {
   return ByteQueue_is_empty(&tx_queue);
   }

bool UART_write_string (
   const char *string)
   {
   bool successful = false;

   // check if there is enough space left in the tx queue
   if (strlen(string) <= ByteQueue_spaceRemaining(&tx_queue))
      {  // there is enough space in the queue
      // push all bytes onto the queue
      const char *byte_ptr = string;
      while ((*byte_ptr) != 0)
         ByteQueue_push(*byte_ptr++, &tx_queue);

      successful = true;
      }

   return successful;
   }

bool UART_write_stringP (
   PGM_P string)
{
    bool successful = false;

    // check if there is enough space left in the tx queue
    if (strlen(string) <= ByteQueue_spaceRemaining(&tx_queue))
        {  // there is enough space in the queue
        // push all bytes onto the queue
        PGM_P cp = string;
        char ch = 0;
        do {
            ch = pgm_read_byte(cp);
            ++cp;
            if (ch != 0) {
                ByteQueue_push(ch, &tx_queue);
            }
        } while (ch != 0);

        successful = true;
        }

   return successful;
}

bool UART_read_string (
   StringBuffer *str_buf)
   {
   bool string_is_complete = false;

   char str_byte;
   if (UART_read_byte(&str_byte))
      {
      if (str_byte == 0x0A)
         string_is_complete = true;
      else
         StringBuffer_append_byte(str_byte, str_buf);
      }

   return string_is_complete;
   }

