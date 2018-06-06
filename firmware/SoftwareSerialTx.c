//
//  Software Serial Transmit
//
//   Uses SystemTime tick as baud clock.
//
//  Pin usage:
//      serial data out pins specified by port and bit passed to SoftwareSerialTx_open()
//

#include "SoftwareSerialTx.h"

#include <avr/io.h>
#include "ByteQueue.h"
#include "SystemTime.h"

#include "UART_async.h"

#define NUM_CHANNELS 2

typedef enum TxState_enum {
    ts_idle,
    ts_sendingDataBits,
    ts_sendingStopBit
} TxState;
// channel descriptor
typedef struct TxDescriptor_struct {
    bool isEnabled;
    volatile TxState txState;
    volatile ByteQueueElement dataByte;
    volatile uint8_t bitNumber;
    IOPortBitfield_t txBit;
    ByteQueue_t *txQueue;
} TxDescriptor;
static TxDescriptor channels[NUM_CHANNELS];

ByteQueue_define(100, txQueue0, static);
ByteQueue_define(100, txQueue1, static);

static void setTxBit (
    const uint8_t bit,
    IOPortBitfield_t *txBit)
{
    if (bit != 0) {
        IOPortBitfield_set(txBit);
    } else {
        IOPortBitfield_clear(txBit);
    }
}

static void systemTimeTickTask (void)
{
    for (int channelIndex = 0; channelIndex < NUM_CHANNELS; ++channelIndex) {
        TxDescriptor *channel = &channels[channelIndex];
        if (channel->isEnabled) {
            switch (channel->txState) {
                case ts_idle:
                    if (!ByteQueue_is_empty(channel->txQueue)) {
                        // issue start bit
                        setTxBit(0, &channel->txBit);
                        channel->dataByte = ByteQueue_pop(channel->txQueue);
                        channel->bitNumber = 1;
                        channel->txState = ts_sendingDataBits;
                    }
                    break;
                case ts_sendingDataBits:
                    setTxBit(channel->dataByte & 1, &channel->txBit);
                    if (channel->bitNumber == 8) {
                        channel->txState = ts_sendingStopBit;
                    } else {
                        ++channel->bitNumber;
                        channel->dataByte >>= 1;
                    }
                    break;
                case ts_sendingStopBit:
                    setTxBit(1, &channel->txBit);
                    channel->txState = ts_idle;
                    break;
            }
        }
    }
}

void SoftwareSerialTx_Initialize (void)
{
    for (int channelIndex = 0; channelIndex < NUM_CHANNELS; ++channelIndex) {
        TxDescriptor *channel = &channels[channelIndex];
        channel->isEnabled = false;
        switch (channelIndex) {
            case 0: channel->txQueue = &txQueue0;   break;
            case 1: channel->txQueue = &txQueue1;   break;
        }
    }
    SystemTime_registerForTickNotification(systemTimeTickTask);
}

void SoftwareSerialTx_open (
    const uint8_t channelIndex,
    const IOPortBitfield_PortSelection port,
    const uint8_t pin)
{
    TxDescriptor *channel = &channels[channelIndex];

    IOPortBitfield_t *txBit = &channel->txBit;
    IOPortBitfield_init(port, pin, 1, true, txBit);
    IOPortBitfield_set(txBit);

    channel->isEnabled = false;
    ByteQueue_clear(channel->txQueue);
}

void SoftwareSerialTx_enable (
    const uint8_t channelIndex)
{
    TxDescriptor *channel = &channels[channelIndex];
    channel->isEnabled = true;
    channel->txState = ts_idle;
}

void SoftwareSerialTx_disable (
    const uint8_t channelIndex)
{
    channels[channelIndex].isEnabled = false;
}

bool SoftwareSerialTx_isIdle (
    const uint8_t channelIndex)
{
    TxDescriptor *channel = &channels[channelIndex];
    return ((channel->txState == ts_idle) && ByteQueue_is_empty(channel->txQueue));
}

uint16_t SoftwareSerialTx_availableSpace (
    const uint8_t channelIndex)
{
    ByteQueue_t *txQueue = channels[channelIndex].txQueue;
    return txQueue->capacity - txQueue->length;
}

void SoftwareSerialTx_send (
    const uint8_t channelIndex,
    const char* text)
{
    TxDescriptor *channel = &channels[channelIndex];
    if (channel->isEnabled) {
        ByteQueue_t *txQueue = channel->txQueue;
        const char* cp = text;
        char ch;
        while ((ch = *cp++) != 0) {
            ByteQueue_push((ByteQueueElement)ch, txQueue);
        }
    }
}

void SoftwareSerialTx_sendCS (
    const uint8_t channelIndex,
    const CharString_t* text)
{
    CharStringSpan_t textSpan;
    CharStringSpan_init(text, &textSpan);
    SoftwareSerialTx_sendCSS(channelIndex, &textSpan);
}

void SoftwareSerialTx_sendCSS (
    const uint8_t channelIndex,
    const CharStringSpan_t* text)
{
    TxDescriptor *channel = &channels[channelIndex];
    if (channel->isEnabled) {
        ByteQueue_t *txQueue = channel->txQueue;
        CharString_Iter iter = CharStringSpan_begin(text);
        CharString_Iter end = CharStringSpan_end(text);
        while (iter != end) {
            const char ch = *iter++;
            ByteQueue_push((ByteQueueElement)ch, txQueue);
        }
    }
}

bool SoftwareSerialTx_sendP (
    const uint8_t channelIndex,
    PGM_P string)
{
    bool successful = false;

    TxDescriptor *channel = &channels[channelIndex];
    if (channel->isEnabled) {
        ByteQueue_t *txQueue = channel->txQueue;
        // check if there is enough space left in the tx queue
        if (strlen_P(string) <= ByteQueue_spaceRemaining(txQueue))
            {  // there is enough space in the queue
            // push all bytes onto the queue
            PGM_P cp = string;
            char ch = 0;
            do {
                ch = pgm_read_byte(cp);
                ++cp;
                if (ch != 0) {
                    ByteQueue_push(ch, txQueue);
                }
            } while (ch != 0);

            successful = true;
        }
    }

   return successful;
}

void SoftwareSerialTx_sendChar (
    const uint8_t channelIndex,
    const char ch)
{
    TxDescriptor *channel = &channels[channelIndex];
    if (channel->isEnabled) {
        ByteQueue_push((ByteQueueElement)ch, channel->txQueue);
    }
}

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
void SoftwareSerialTx_reportHighwater (void)
{
    ByteQueue_reportHighwater(PSTR("SSTX0"), channels[0].txQueue);
    ByteQueue_reportHighwater(PSTR("SSTX1"), channels[1].txQueue);
}
#endif
