//
//  SIM800 Quad-band GSM/GPRS module interface
//
//  Uses SoftwareSerialTx channel 0
//
#ifndef SIM800_H
#define SIM800_H

#include "ByteQueue.h"
#include <avr/pgmspace.h>
#include "CharStringSpan.h"
#include "IOPortBitfield.h"
#include <stdint.h>

typedef enum SIM800_moduleStatusEnum {
    SIM800_ms_off,
    SIM800_ms_initializing,
    SIM800_ms_readyForCommand,
    SIM800_ms_processingCommand,
    SIM800_ms_poweringDown
} SIM800_moduleStatus;

typedef enum SIM800_ResponseMessage_enum {
    rm_CLOSE_OK,
    rm_CLOSED,
    rm_CONNECT_FAIL,
    rm_CONNECT_OK,
    rm_Call_Ready,
    rm_ERROR,
    rm_NORMAL_POWER_DOWN,
    rm_OK,
    rm_RDY,
    rm_SEND_FAIL,
    rm_SEND_OK,
    rm_SHUT_OK,
    rm_SMS_Ready,
    rm_UNDERVOLTAGE_POWER_DOWN,
    rm_unrecognized,
    rm_noResponseYet
} SIM800_ResponseMessage;

typedef enum SIM800_IPState_enum {
    ips_CONNECT_OK,
    ips_IP_CONFIG,
    ips_IP_GPRSACT,
    ips_IP_INITIAL,
    ips_IP_START,
    ips_IP_STATUS,
    ips_PDP_DEACT,
    ips_SERVER_LISTENING,
    ips_TCP_CLOSED,
    ips_TCP_CLOSING,
    ips_TCP_CONNECTING,
    ips_UDP_CLOSED,
    ips_UDP_CLOSING,
    ips_UDP_CONNECTING,
    ips_unknown
} SIM800_IPState;

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minutes;
    uint8_t seconds;
} SIM800_NetworkTime;

typedef struct SIM800_CIPACKData_struc {
    uint32_t dataSent;
    uint32_t dataConfirmed;
    uint32_t dataNotConfirmed;
} SIM800_CIPACKData;

// callbacks for responses
typedef void (*SIM800_ResponseMessageCallback)(
    const SIM800_ResponseMessage msg);
typedef void (*SIM800_IPStateCallback)(
    const SIM800_IPState ipState);
typedef void (*SIM800_IPAddressCallback)(
    const CharString_t *ipAddress);
typedef void (*SIM800_IPDataCallback)(
    const CharString_t *ipData);
typedef void (*SIM800_CIPACKCallback)(
    const SIM800_CIPACKData *cipackData);
typedef void (*SIM800_DataAcceptCallback)(
    const uint16_t dataSent);
typedef void (*SIM800_PDPDeactCallback)(
    void);
typedef void (*SIM800_CGATTCallback)(
    const bool gprsAttached);
typedef void (*SIM800_SMSMessageReceivedCallback)(
    const int16_t msgID,
    const CharString_t *messageStatus, 
    const CharString_t *phoneNumber, 
    const CharString_t *message);
typedef void (*SIM800_CFUNCallback)(
    const int16_t funclevel);
typedef void (*SIM800_CSQCallback)(
    const int16_t sigStrength);
typedef void (*SIM800_CREGCallback)(
    const int16_t reg);
typedef void (*SIM800_CMTICallback)(
    const int16_t msgID);
typedef void (*SIM800_CPINCallback)(
    const CharStringSpan_t *cpinStatus);
typedef void (*SIM800_CCLKCallback)(
    const SIM800_NetworkTime *time);
typedef void (*SIM800_CBCCallback)(
    const uint8_t chargeStatus,
    const uint8_t percent,
    const uint16_t millivolts);
typedef void (*SIM800_promptCallback)(
    void);

extern void SIM800_Initialize (
    ByteQueue_t *rxQ,
    IOPortBitfield_PortSelection txPort,
    uint8_t txPin);

extern void SIM800_task (void);

extern void SIM800_powerOn (void);
extern void SIM800_powerOff (void);

extern SIM800_moduleStatus SIM800_status (void);

extern void SIM800_setResponseMessageCallback (
    SIM800_ResponseMessageCallback cb);
extern void SIM800_setIPStateCallback (
    SIM800_IPStateCallback cb);
extern void SIM800_setIPDataCallback (
    SIM800_IPDataCallback cb);
extern void SIM800_setCIPACKCallback(
    SIM800_CIPACKCallback cb);
extern void SIM800_setDataAcceptCallback (
    SIM800_DataAcceptCallback cb);
extern void SIM800_setIPAddressCallback (
    SIM800_IPAddressCallback cb);
extern void SIM800_setPDPDeactCallback (
    SIM800_PDPDeactCallback cb);
extern void SIM800_setCGATTCallback (
    SIM800_CGATTCallback cb);
extern void SIM800_setSMSMessageReceivedCallback (
    SIM800_SMSMessageReceivedCallback cb);
extern void SIM800_setCFUNCallback (
    SIM800_CFUNCallback cb);
extern void SIM800_setCSQCallback (
    SIM800_CSQCallback cb);
extern void SIM800_setCREGCallback (
    SIM800_CREGCallback cb);
extern void SIM800_setCMTICallback (
    SIM800_CMTICallback cb);
extern void SIM800_setCPINCallback (
    SIM800_CPINCallback cb);
extern void SIM800_setCCLKCallback (
    SIM800_CCLKCallback cb);
extern void SIM800_setCBCCallback (
    SIM800_CBCCallback cb);
extern void SIM800_setPromptCallback (
    SIM800_promptCallback cb);

extern uint16_t SIM800_availableSpaceForSend (void);

extern void SIM800_sendStringP (
    PGM_P str);
extern void SIM800_sendStringCS (
    const CharString_t* str);
extern void SIM800_sendStringCSS (
    const CharStringSpan_t* str);

// sends the given string and a CR
extern void SIM800_sendLineP (
    PGM_P str);
extern void SIM800_sendLineCS (
    const CharString_t* str);

// sends the given string as hex pairs
extern void SIM800_sendHex (
    const uint8_t* data);
void SIM800_sendHexP (
    PGM_P data);

extern void SIM800_sendCtrlZ (void);

#endif  // SIM800_H
