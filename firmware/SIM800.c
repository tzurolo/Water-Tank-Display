//
//  SIM800 Quad-band GSM/GPRS module interface
//
//  This implementation is targeted to the Adafruit FONA board, and uses
//  software serial with the same pinout as the Adafruit Feather FONA

//  I/O Pin usage
//      D2        -> Power Status (optional)
//      F1        -> OnKey
//      Tx pin specified from SIM800_Initialize
//      Rx pin not specified here, input comes from rxQ from SIM800_Initialize
//
#include "SIM800.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "ByteQueue.h"
#include "SoftwareSerialTx.h"
#include "Console.h"
#include "SystemTime.h"
#include "StringUtils.h"

#define USE_POWER_STATE 0
#define DEBUG_TRACE 0

#define TX_CHAN_INDEX 0

#define ONKEY_PIN           PF1
#define ONKEY_OUTPORT       PORTF
#define ONKEY_DIR           DDRF

#define POWERSTATE_PIN      PD2
#define POWERSTATE_INPORT   PIND
#define POWERSTATE_DIR      DDRD

#define RESPONSE_BUFFER_LENGTH 200

char rmCLOSE_OK[]           PROGMEM = "CLOSE OK";
char rmCLOSED[]             PROGMEM = "CLOSED";
char rmCONNECT_FAIL[]       PROGMEM = "CONNECT FAIL";
char rmCONNECT_OK[]         PROGMEM = "CONNECT OK";
char rmCall_Ready[]         PROGMEM = "Call Ready";
char rmERROR[]              PROGMEM = "ERROR";
char rmNORMAL_POWER_DOWN[]  PROGMEM = "NORMAL POWER DOWN";
char rmOK[]                 PROGMEM = "OK";
char rmRDY[]                PROGMEM = "RDY";
char rmSEND_FAIL[]          PROGMEM = "SEND FAIL";
char rmSEND_OK[]            PROGMEM = "SEND OK";
char rmSHUT_OK[]            PROGMEM = "SHUT OK";
char rmSMS_Ready[]          PROGMEM = "SMS Ready";

// table must be maintained in ASCII collation order, and
// in sync with SIM800_ResponseMessage enum
PGM_P SIM800_ResponseMessageTable[] PROGMEM = 
{
    rmCLOSE_OK,
    rmCLOSED,
    rmCONNECT_FAIL,
    rmCONNECT_OK,
    rmCall_Ready,
    rmERROR,
    rmNORMAL_POWER_DOWN,
    rmOK,
    rmRDY,
    rmSEND_FAIL,
    rmSEND_OK,
    rmSHUT_OK,
    rmSMS_Ready
};
static const int SIM800_ResponseMessageTableSize = sizeof(SIM800_ResponseMessageTable) / sizeof(PGM_P);

typedef enum PlusMessage_enum {
    pm_CBC,
    pm_CCLK,
    pm_CFUN,
    pm_CGATT,
    pm_CIPACK,
    pm_CMGL,
    pm_CMGR,
    pm_CMGS,
    pm_CMTI,
    pm_CPIN,
    pm_CREG,
    pm_CSQ,
    pm_PDP,
    pm_unrecognized
} PlusMessage;

char pmCBC[]    PROGMEM = "CBC";
char pmCCLK[]   PROGMEM = "CCLK";
char pmCFUN[]   PROGMEM = "CFUN";
char pmCGATT[]  PROGMEM = "CGATT";
char pmCIPACK[] PROGMEM = "CIPACK";
char pmCMGL[]   PROGMEM = "CMGL";
char pmCMGR[]   PROGMEM = "CMGR";
char pmCMGS[]   PROGMEM = "CMGS";
char pmCMTI[]   PROGMEM = "CMTI";
char pmCPIN[]   PROGMEM = "CPIN";
char pmCREG[]   PROGMEM = "CREG";
char pmCSQ[]    PROGMEM = "CSQ";
char pmPDP []   PROGMEM = "PDP";

// table must be maintained in ASCII collation order, and
// in sync with PlusMessage enum
PGM_P plusMessageTable[] PROGMEM = 
{
    pmCBC,
    pmCCLK,
    pmCFUN,
    pmCGATT,
    pmCIPACK,
    pmCMGL,
    pmCMGR,
    pmCMGS,
    pmCMTI,
    pmCPIN,
    pmCREG,
    pmCSQ,
    pmPDP
};
static const int plusMessageTableSize = sizeof(plusMessageTable) / sizeof(PGM_P);

char ipsCONNECT_OK[]        PROGMEM = "CONNECT OK";
char ipsIP_CONFIG[]         PROGMEM = "IP CONFIG";
char ipsIP_GPRSACT[]        PROGMEM = "IP GPRSACT";
char ipsIP_INITIAL[]        PROGMEM = "IP INITIAL";
char ipsIP_START[]          PROGMEM = "IP START";
char ipsIP_STATUS[]         PROGMEM = "IP STATUS";
char ipsPDP_DEACT[]         PROGMEM = "PDP DEACT";
char ipsSERVER_LISTENING[]  PROGMEM = "SERVER LISTENING";
char ipsTCP_CLOSED[]        PROGMEM = "TCP CLOSED";
char ipsTCP_CLOSING[]       PROGMEM = "TCP CLOSING";
char ipsTCP_CONNECTING[]    PROGMEM = "TCP CONNECTING";
char ipsUDP_CLOSED[]        PROGMEM = "UDP CLOSED";
char ipsUDP_CLOSING[]       PROGMEM = "UDP CLOSING";
char ipsUDP_CONNECTING[]    PROGMEM = "UDP CONNECTING";

// table must be maintained in ASCII collation order, and
// in sync with IPState enum
PGM_P ipStateTable[] PROGMEM = 
{
    ipsCONNECT_OK,
    ipsIP_CONFIG,
    ipsIP_GPRSACT,
    ipsIP_INITIAL,
    ipsIP_START,
    ipsIP_STATUS,
    ipsPDP_DEACT,
    ipsSERVER_LISTENING,
    ipsTCP_CLOSED,
    ipsTCP_CLOSING,
    ipsTCP_CONNECTING,
    ipsUDP_CLOSED,
    ipsUDP_CLOSING,
    ipsUDP_CONNECTING
};
static const int ipStateTableSize = sizeof(ipStateTable) / sizeof(PGM_P);

typedef enum responseProcessorStateEnum {
    rps_interpret,
    rps_readIPData,
    rps_raw
} responseProcessorState;

typedef enum moduleStateEnum {
    ms_off,
    ms_waitingForPowerStateResponse,
    ms_waitingForCommand,
    ms_executingCommand,
    ms_powerRetryDelay,
} ModuleState;

static bool powerCommand;
static ModuleState mState;
static ByteQueue_t* rxQueue;
CharString_define(RESPONSE_BUFFER_LENGTH, SIM800Response);
SIM800_ResponseMessage responseMsg;
static SystemTime_t powerRetryTime;
static char prevInByte;
static responseProcessorState rpState;
static uint16_t ipDataLength;
static PlusMessage latestPlusMessage;
static int16_t smsMsgID;
CharString_define(10, smsMsgStatus);
CharString_define(20, smsPhoneNumber);

// registered callbacks
static SIM800_ResponseMessageCallback responseCallback;
static SIM800_IPStateCallback ipStateCallback;
static SIM800_IPAddressCallback ipAddressCallback;
static SIM800_IPDataCallback ipDataCallback;
static SIM800_CIPACKCallback cipackCallback;
static SIM800_DataAcceptCallback dataAcceptCallback;
static SIM800_PDPDeactCallback pdpDeactCallback;
static SIM800_CGATTCallback cgattCallback;
static SIM800_SMSMessageReceivedCallback smsMessageReceivedCallback;
static SIM800_CFUNCallback CFUNCallback;
static SIM800_CSQCallback CSQCallback;
static SIM800_CREGCallback CREGCallback;
static SIM800_CMTICallback CMTICallback;
static SIM800_CPINCallback CPINCallback;
static SIM800_CCLKCallback CCLKCallback;
static SIM800_CBCCallback CBCCallback;
static SIM800_promptCallback promptCallback;

static void readCBC (
    CharStringSpan_t *str)
{
    bool isValid;
    int16_t status;
    StringUtils_skipWhitespace(str);
    StringUtils_scanInteger(str, &isValid, &status, str);
    if (isValid && (CharStringSpan_front(str) == ',')) {
        int16_t percent;
        CharStringSpan_incrBegin(str);   // step over ','
        StringUtils_scanInteger(str, &isValid, &percent, str);
        if (isValid && (CharStringSpan_front(str) == ',')) {
            int16_t millivolts;
            CharStringSpan_incrBegin(str);   // step over ','
            StringUtils_scanInteger(str, &isValid, &millivolts, NULL);
            if (isValid && (CBCCallback != 0)) {
                CBCCallback(status, percent, millivolts);
            }
        }
    }
}

static void scanCCLKField (
    CharStringSpan_t *str,
    uint8_t *fieldValue)
{
    bool isValid;
    int16_t scannedValue;
    StringUtils_scanInteger(str, &isValid, &scannedValue, str);
    if (isValid) {
        *fieldValue = scannedValue;
        CharStringSpan_incrBegin(str);  // skip delimiter
    } else {
        *fieldValue = 0;
    }
}

static void readCCLK (
    const CharStringSpan_t *str)
{
    // scan time string
    CharStringSpan_t timeString;
    StringUtils_scanQuotedString(str, &timeString, NULL);

    // parse time string to produce NetworkTime
    // format is “yy/mm/mm,hh:mm:ss+tz”
    SIM800_NetworkTime currentNetworkTime;
    scanCCLKField(&timeString, &currentNetworkTime.year);
    scanCCLKField(&timeString, &currentNetworkTime.month);
    scanCCLKField(&timeString, &currentNetworkTime.day);
    scanCCLKField(&timeString, &currentNetworkTime.hour);
    scanCCLKField(&timeString, &currentNetworkTime.minutes);
    scanCCLKField(&timeString, &currentNetworkTime.seconds);

    if (CCLKCallback != 0) {
        CCLKCallback(&currentNetworkTime);
    }
}

static void readCFUN (
    CharStringSpan_t *str)
{
    StringUtils_skipWhitespace(str);
    bool isValid;
    int16_t funclevel;
    StringUtils_scanInteger(str, &isValid, &funclevel, NULL);
    if (isValid) {

#if DEBUG_TRACE
        CharString_define(10, msgstr);
        CharString_copyP(PSTR("Funclevel: "), &msgstr);
        StringUtils_appendDecimal(funclevel, 1, 0, &msgstr);
        Console_printCS(&msgstr);
#endif

        if (CFUNCallback != 0) {
            CFUNCallback(funclevel);
        }
    }
}

static void readCGATT (
    CharStringSpan_t *str)
{
    StringUtils_skipWhitespace(str);
    bool isValid;
    int16_t value;
    StringUtils_scanInteger(str, &isValid, &value, NULL);
    if (isValid) {

#if DEBUG_TRACE
        CharString_define(10, msgidstr);
        CharString_copyP(PSTR("CGATT: "), &msgidstr);
        StringUtils_appendDecimal(value, 1, 0, &msgidstr);
        Console_printCS(&msgidstr);
#endif

        if (cgattCallback != 0) {
            cgattCallback(value != 0);
        }
    }
}

static void readCIPACK (
    CharStringSpan_t *str)
{
    bool isValid;
    SIM800_CIPACKData cipackData;
    StringUtils_skipWhitespace(str);
    StringUtils_scanIntegerU32(str, &isValid, &cipackData.dataSent, str);
    if (isValid) {
        if (CharStringSpan_front(str) == ',') {
            CharStringSpan_incrBegin(str);
        } else {
            isValid = false;
        }
    }
    if (isValid) {
        StringUtils_scanIntegerU32(str, &isValid, &cipackData.dataConfirmed, str);
    }
    if (isValid) {
        if (CharStringSpan_front(str) == ',') {
            CharStringSpan_incrBegin(str);
        } else {
            isValid = false;
        }
    }
    if (isValid) {
        StringUtils_scanIntegerU32(str, &isValid, &cipackData.dataNotConfirmed, NULL);
    }
    if (isValid) {
#if DEBUG_TRACE
        CharString_define(30, dataStr);
        CharString_copyP(PSTR("CIPACK: "), &dataStr);
        StringUtils_appendDecimal32(cipackData.dataSent, 1, 0, &dataStr);
        CharString_appendP(PSTR(","),  &dataStr);
        StringUtils_appendDecimal32(cipackData.dataConfirmed, 1, 0, &dataStr);
        CharString_appendP(PSTR(","),  &dataStr);
        StringUtils_appendDecimal32(cipackData.dataNotConfirmed, 1, 0, &dataStr);
        Console_printCS(&dataStr);
#endif

        if (cipackCallback != 0) {
            cipackCallback(&cipackData);
        }
    }
}

static void readCMGR (
    CharStringSpan_t *str)
{
    CharStringSpan_t smsSpan;
    StringUtils_scanQuotedString(str, &smsSpan, str);
    CharString_copyIters(
        CharStringSpan_begin(&smsSpan), CharStringSpan_end(&smsSpan), &smsMsgStatus);
    StringUtils_scanQuotedString(str, &smsSpan, str);
    CharString_copyIters(
        CharStringSpan_begin(&smsSpan), CharStringSpan_end(&smsSpan), &smsPhoneNumber);
    rpState = rps_raw;
}

static void readCMGL (
    CharStringSpan_t *str)
{
    bool isValid;
    StringUtils_skipWhitespace(str);
    StringUtils_scanInteger(str, &isValid, &smsMsgID, str);
    readCMGR(str);
}

static void readCMGS (
    CharStringSpan_t *str)
{
}

static void readCMTI (
    CharStringSpan_t *str)
{
    CharStringSpan_t typeStr;
    StringUtils_scanQuotedString(str, &typeStr, str);
    if (CharStringSpan_front(str) == ',') {
        bool isValid;
        int16_t msgid;
        CharStringSpan_incrBegin(str);
        StringUtils_scanInteger(str, &isValid, &msgid, NULL);
        if (isValid) {

#if DEBUG_TRACE
            CharString_define(10, msgidstr);
            CharString_copyP(PSTR("SMS id: "), &msgidstr);
            StringUtils_appendDecimal(msgid, 1, 0, &msgidstr);
            Console_printCS(&msgidstr);
#endif

            if (CMTICallback != 0) {
                CMTICallback(msgid);
            }
        }
    }
}

static void readCPIN (
    CharStringSpan_t *str)
{
    CharStringSpan_t statusStr;
    StringUtils_scanQuotedString(str, &statusStr, NULL);

    if (CPINCallback != 0) {
        CPINCallback(&statusStr);
    }
}

static void readCREG (
    CharStringSpan_t *str)
{
    bool isValid;
    int16_t reg;
    StringUtils_skipWhitespace(str);
    StringUtils_scanInteger(str, &isValid, &reg, str);
    if (isValid && (CharStringSpan_front(str) == ',')) {
        CharStringSpan_incrBegin(str);   // step over ','
        StringUtils_scanInteger(str, &isValid, &reg, NULL);
        if (isValid) {

#if DEBUG_TRACE
            CharString_define(10, msgstr);
            CharString_copyP(PSTR("Reg: "), &msgstr);
            StringUtils_appendDecimal(reg, 1, 0, &msgstr);
            Console_printCS(&msgstr);
#endif

            if (CREGCallback != 0) {
                CREGCallback(reg);
            }
        }
    }
}

static void readCSQ (
    CharStringSpan_t *str)
{
    bool isValid;
    int16_t sigStrength;
    StringUtils_skipWhitespace(str);
    StringUtils_scanInteger(str, &isValid, &sigStrength, NULL);
    if (isValid) {

#if DEBUG_TRACE
        CharString_define(10, msgstr);
        CharString_copyP(PSTR("Sig: "), &msgstr);
        StringUtils_appendDecimal(sigStrength, 1, 0, &msgstr);
        Console_printCS(&msgstr);
#endif

        if (CSQCallback != 0) {
            CSQCallback(sigStrength);
        }
    }
}

static void readPDP (
    CharStringSpan_t *str)
{
    StringUtils_skipWhitespace(str);
    if (CharStringSpan_equalsP(str, PSTR("DEACT"))) {
        if (pdpDeactCallback != 0) {
            pdpDeactCallback();
        }
    }
}

static void processPlusMessage (
    CharStringSpan_t *plusMsg)
{
    // scan message identifier
    CharStringSpan_t msg;
    StringUtils_scanDelimitedString('+', ':', plusMsg, &msg, plusMsg);
    const int msgIndex = StringUtils_lookupString(
        &msg, plusMessageTable, plusMessageTableSize);
    latestPlusMessage = ((PlusMessage)msgIndex);
    switch (latestPlusMessage) {
        case pm_CBC     : readCBC(plusMsg);     break;
        case pm_CCLK    : readCCLK(plusMsg);    break;
        case pm_CFUN    : readCFUN(plusMsg);    break;
        case pm_CGATT   : readCGATT(plusMsg);   break;
        case pm_CIPACK  : readCIPACK(plusMsg);  break;
        case pm_CMGL    : readCMGL(plusMsg);    break;
        case pm_CMGR    : smsMsgID = 0;
                          readCMGR(plusMsg);    break;
        case pm_CMGS    : readCMGS(plusMsg);    break;
        case pm_CMTI    : readCMTI(plusMsg);    break;
        case pm_CPIN    : readCPIN(plusMsg);    break;
        case pm_CREG    : readCREG(plusMsg);    break;
        case pm_CSQ     : readCSQ(plusMsg);     break;
        case pm_PDP     : readPDP(plusMsg);     break;
        case pm_unrecognized :
            Console_printP(PSTR("unrec:"));
            Console_printCSS(plusMsg);
            break;
    }
}

static void processIPState (
    const CharStringSpan_t *stateStr)
{
    const int msgIndex = StringUtils_lookupString(
        stateStr, ipStateTable, ipStateTableSize);
    const SIM800_IPState ipState = ((SIM800_IPState)msgIndex);
    if (ipStateCallback != 0) {
        ipStateCallback(ipState);
    }
}

static void processResponseMessage (
    const CharStringSpan_t *responseMsgStr)
{
    const int msgIndex = StringUtils_lookupString(
        responseMsgStr, SIM800_ResponseMessageTable, SIM800_ResponseMessageTableSize);
    responseMsg = ((SIM800_ResponseMessage)msgIndex);
    switch (responseMsg) {
        case rm_Call_Ready          :
            break;
        case rm_CONNECT_OK          :
            Console_printP(PSTR("Connect OK"));
            break;
        case rm_CONNECT_FAIL        :
            Console_printP(PSTR("Connect FAIL"));
            break;
        case rm_CLOSED              :
            Console_printP(PSTR("Closed"));
            break;
        case rm_CLOSE_OK            :
        case rm_OK                  :
        case rm_SEND_OK             :
        case rm_SHUT_OK             :
#if DEBUG_TRACE
            Console_printP(PSTR("Successful"));
#endif
            break;
        case rm_SEND_FAIL           :
        case rm_ERROR               :
            Console_printP(PSTR("Failed"));
            break;
        case rm_NORMAL_POWER_DOWN   :
            Console_printP(PSTR("Powered Down!"));
            break;
        case rm_RDY                 :
            Console_printP(PSTR("Ready!"));
            break;
        case rm_SMS_Ready           :
            break;
        case rm_unrecognized        :
            Console_printP(PSTR("unrec:"));
            Console_printCSS(responseMsgStr);
            break;
        case rm_noResponseYet       :
            break;
    }
    if (responseCallback != 0) {
        responseCallback(responseMsg);
    }
}

static void interpretResponse (
    const CharString_t *response)
{
    if (!CharString_isEmpty(response)) {
        // look at first char
        const char firstChar = CharString_at(response, 0);
        if (firstChar == '+') {
            CharStringSpan_t responseSpan;
            CharStringSpan_init(response, &responseSpan);
            processPlusMessage(&responseSpan);
        } else if ((firstChar >= '0') && (firstChar <= '9')) {
            // TCP/IP address
#if DEBUG_TRACE
            Console_printP(PSTR("Got TCP/IP address: "));
            Console_printCS(response);
#endif
            if (ipAddressCallback != 0) {
                ipAddressCallback(response);
            }
        } else if ((firstChar == 'S') && 
                   CharString_startsWithP(response, PSTR("STATE: "))) {
            CharStringSpan_t ipStateStr;
            CharStringSpan_initRight(response, 7, &ipStateStr);
            processIPState(&ipStateStr);
        } else if ((firstChar == 'D') && 
                   CharString_startsWithP(response, PSTR("DATA ACCEPT:"))) {
            if (dataAcceptCallback != 0) {
                dataAcceptCallback(0);  // TODO: need to read number
            }
        } else {
            CharStringSpan_t responseSpan;
            CharStringSpan_init(response, &responseSpan);
            processResponseMessage(&responseSpan);
        }
    }
}

// returns true if response is a complete non-terminated response:
// "> " prompt
// +IPD,n:... TCP/IP data
//
static bool interpretNonterminatedResponse (
    const CharString_t *response)
{
    bool isNonterminatedResponse = false;

    const int responseLength = CharString_length(response);
    if (responseLength >= 2) {
        const char firstChar = CharString_at(response, 0);
        const char secondChar = CharString_at(response, 1);
        if ((firstChar == '>') && (secondChar == ' ')) {
            // got prompt
#if DEBUG_TRACE
            Console_printP(PSTR("Got prompt"));
#endif
            if (promptCallback != 0) {
                promptCallback();
            }
            isNonterminatedResponse = true;
        } else if ((firstChar == '+') &&
                   (secondChar == 'I') && 
                   (CharString_at(response, responseLength-1) == ':') &&
                   (CharString_startsWithP(response, PSTR("+IPD,")))) {
            // could be IP data
            CharStringSpan_t ipdStr;
            CharStringSpan_initRight(response, 5, &ipdStr);
            bool isValidLength = false;
            int16_t dataLength = 0;
            StringUtils_scanInteger(&ipdStr, &isValidLength, &dataLength, &ipdStr);
            if (isValidLength && (CharStringSpan_front(&ipdStr) == ':')) {
                // getting TCP/IP data
                ipDataLength = dataLength;
                rpState = rps_readIPData;
                isNonterminatedResponse = true;
            }
        }
    }

    return isNonterminatedResponse;
}

static void processResponseBytes (void)
{
    ByteQueueElement inByte;
    for (uint8_t numBytes = ByteQueue_length(rxQueue); numBytes > 0;
            --numBytes) {
        inByte = ByteQueue_pop(rxQueue);
        switch (rpState) {
            case rps_interpret :
                if (inByte == 13) {

#if DEBUG_TRACE
                    // limit console print message to 65 chars
                    CharString_define(65, msg);
                    CharString_copyP(PSTR("SIM800: '"), &msg);
                    CharString_appendCS(&SIM800Response, &msg);
                    CharString_appendC('\'', &msg);
                    Console_printCS(&msg);
#endif

                    interpretResponse(&SIM800Response);
                    CharString_clear(&SIM800Response);
                } else if (!((inByte == 10) && (prevInByte == 13))) {// discard LF if it immediately follows CR
                    CharString_appendC((char)inByte, &SIM800Response);
                    if (interpretNonterminatedResponse(&SIM800Response)) {
                        CharString_clear(&SIM800Response);
                    }
                }
                break;
            case rps_readIPData :
                CharString_appendC((char)inByte, &SIM800Response);
                if (CharString_length(&SIM800Response) >= ipDataLength) {
                    // got all IP data

#if DEBUG_TRACE
                    CharString_define(30, msg);
                    CharString_copyP(PSTR("Got IP data ("), &msg);
                    StringUtils_appendDecimal(ipDataLength, 0, 0, &msg);
                    CharString_appendP(PSTR(") '"), &msg);
                    CharString_appendCS(&SIM800Response, &msg);
                    CharString_appendC('\'', &msg);
                    Console_printCS(&msg);
#endif

                    if (ipDataCallback != 0) {
                        ipDataCallback(&SIM800Response);
                    }
                    CharString_clear(&SIM800Response);
                    rpState = rps_interpret;
                }
                break;
            case rps_raw :
                if (inByte == 13) {
                    // got raw data line
                    if ((latestPlusMessage == pm_CMGL) ||
                        (latestPlusMessage == pm_CMGR)) {
#if DEBUG_TRACE
                        Console_printCS(&smsPhoneNumber);
                        Console_printCS(&SIM800Response);
#endif

                        if (smsMessageReceivedCallback != 0) {
                            smsMessageReceivedCallback(
                                smsMsgID, &smsMsgStatus, &smsPhoneNumber, &SIM800Response);
                        }
                    }
                    CharString_clear(&SIM800Response);
                    rpState = rps_interpret;
                } else if (!((inByte == 10) && (prevInByte == 13))) {// discard LF if it immediately follows CR
                    CharString_appendC((char)inByte, &SIM800Response);
                }
                break;
            default :
                break;
        }

        prevInByte = inByte;
    }
}

// handlers for unsolicited responses

#if USE_POWER_STATE
// not using the power state - it's not available on the Feather FONA

static bool readPowerState (void)
{
    return ((POWERSTATE_INPORT & (1 << POWERSTATE_PIN)) != 0);
}
#endif

static void assertOnKey (void)
{
    // make onkey pin an output so low state is asserted
    ONKEY_DIR |= (1 << ONKEY_PIN);
}

static void deassertOnKey (void)
{
    // make onkey pin an input to deassert
    ONKEY_DIR &= ~(1 << ONKEY_PIN);
}

void SIM800_powerOn (void)
{
    powerCommand = true;
}

void SIM800_powerOff (void)
{
    powerCommand = false;
}

void SIM800_Initialize (
    ByteQueue_t *rxQ,
    IOPortBitfield_PortSelection txPort,
    uint8_t txPin)
{
    rxQueue = rxQ;

    // set onkey to low (asserted) state, but set pin as input
    ONKEY_DIR &= ~(1 << ONKEY_PIN);
    ONKEY_OUTPORT &= ~(1 << ONKEY_PIN);

    powerCommand = false;
#if USE_POWER_STATE
    // set power state pin to be input
    POWERSTATE_DIR &= ~(1 << POWERSTATE_PIN);
    mState = (readPowerState())
        ? ms_waitingForCommand      // already powered up
        : ms_off;
#else
    mState = ms_off;    // assume off, powerup sequence will find out
#endif

    responseMsg = rm_noResponseYet;
    prevInByte = 0;
    rpState = rps_interpret;

    responseCallback = 0;
    ipStateCallback = 0;
    ipAddressCallback = 0;
    ipDataCallback = 0;
    cipackCallback = 0;
    dataAcceptCallback = 0;
    pdpDeactCallback = 0;
    cgattCallback = 0;
    smsMessageReceivedCallback = 0;
    CFUNCallback = 0;
    CSQCallback = 0;
    CREGCallback = 0;
    CMTICallback = 0;
    CPINCallback = 0;
    CCLKCallback = 0;
    CBCCallback = 0;
    promptCallback = 0;

    SoftwareSerialTx_Initialize(TX_CHAN_INDEX, txPort, txPin);
    SoftwareSerialTx_enable(TX_CHAN_INDEX);
}

void SIM800_task (void)
{
    processResponseBytes();

    switch (mState) {
        case ms_off :
            if (powerCommand) {
                responseMsg = rm_noResponseYet;
                assertOnKey();
                mState = ms_waitingForPowerStateResponse;
            }
            break;
        case ms_waitingForPowerStateResponse :
            if (responseMsg == rm_RDY) {
                deassertOnKey();
#if DEBUG_TRACE
                Console_printP(PSTR("--- On ---"));
#endif
                if (powerCommand) {
                    mState = ms_waitingForCommand;
                } else {
                    // was trying to power off - try again in a bit
                    SystemTime_futureTime(200, &powerRetryTime);
                    mState = ms_powerRetryDelay;
                }
            } else if (responseMsg == rm_NORMAL_POWER_DOWN) {
                deassertOnKey();
#if DEBUG_TRACE
                Console_printP(PSTR("--- Off ---"));
#endif
                if (powerCommand) {
                    // was trying to power on - try again in a bit
                    SystemTime_futureTime(200, &powerRetryTime);
                    mState = ms_powerRetryDelay;
                } else {
                    mState = ms_off;
                }
            }
            break;
        case ms_waitingForCommand :
            if (!powerCommand) {
                responseMsg = rm_noResponseYet;
                assertOnKey();
                mState = ms_waitingForPowerStateResponse;
            }
            break;
        case ms_powerRetryDelay :
            if (SystemTime_timeHasArrived(&powerRetryTime)) {
                responseMsg = rm_noResponseYet;
                assertOnKey();
                mState = ms_waitingForPowerStateResponse;
            }
            break;
        default:
            // should reboot if we get here
            break;
    }
}

static uint8_t nibbleHex (
    const uint8_t nibbleByte)
{
    uint8_t hex = 0;

    uint8_t nibble = nibbleByte & 0x0F;
    if (nibble >= 10) {
        hex = (nibble - 10) + 'A';
    } else {
        hex = nibble + '0';
    }

    return hex;
}

SIM800_moduleStatus SIM800_status (void)
{
    SIM800_moduleStatus status = SIM800_ms_off;

    switch (mState) {
        case ms_off :
            status = SIM800_ms_off;
            break;
        case ms_waitingForPowerStateResponse :
        case ms_powerRetryDelay :
            status = powerCommand
                ? SIM800_ms_initializing
                : SIM800_ms_poweringDown;
            break;
        case ms_waitingForCommand :
            status = SIM800_ms_readyForCommand;
            break;
        case ms_executingCommand :
            status = SIM800_ms_processingCommand;
            break;
    }

    return status;
}

void SIM800_setResponseMessageCallback (
    SIM800_ResponseMessageCallback cb)
{
    responseCallback = cb;
}

void SIM800_setIPStateCallback (
    SIM800_IPStateCallback cb)
{
    ipStateCallback = cb;
}

void SIM800_setIPAddressCallback (
    SIM800_IPAddressCallback cb)
{
    ipAddressCallback = cb;
}

void SIM800_setIPDataCallback (
    SIM800_IPDataCallback cb)
{
    ipDataCallback = cb;
}

void SIM800_setCIPACKCallback(
    SIM800_CIPACKCallback cb)
{
    cipackCallback = cb;
}

void SIM800_setDataAcceptCallback (
    SIM800_DataAcceptCallback cb)
{
    dataAcceptCallback = cb;
}

void SIM800_setPDPDeactCallback (
    SIM800_PDPDeactCallback cb)
{
    pdpDeactCallback = cb;
}

void SIM800_setCGATTCallback (
    SIM800_CGATTCallback cb)
{
    cgattCallback = cb;
}

void SIM800_setSMSMessageReceivedCallback (
    SIM800_SMSMessageReceivedCallback cb)
{
    smsMessageReceivedCallback = cb;
}

void SIM800_setCFUNCallback (
    SIM800_CFUNCallback cb)
{
    CFUNCallback = cb;
}

void SIM800_setCSQCallback (
    SIM800_CSQCallback cb)
{
    CSQCallback = cb;
}

void SIM800_setCREGCallback (
    SIM800_CREGCallback cb)
{
    CREGCallback = cb;
}

void SIM800_setCMTICallback (
    SIM800_CMTICallback cb)
{
    CMTICallback = cb;
}

void SIM800_setCPINCallback (
    SIM800_CPINCallback cb)
{
    CPINCallback = cb;
}

void SIM800_setCCLKCallback (
    SIM800_CCLKCallback cb)
{
    CCLKCallback = cb;
}

void SIM800_setCBCCallback (
    SIM800_CBCCallback cb)
{
    CBCCallback = cb;
}

void SIM800_setPromptCallback (
    SIM800_promptCallback cb)
{
    promptCallback = cb;
}

uint16_t SIM800_availableSpaceForSend (void)
{
    return SoftwareSerialTx_availableSpace(TX_CHAN_INDEX);
}

void SIM800_sendStringP (
    PGM_P str)
{
#if DEBUG_TRACE
    Console_printP(str);
#endif

    SoftwareSerialTx_sendP(TX_CHAN_INDEX, str);
}

void SIM800_sendStringCS (
    const CharString_t* str)
{
    SoftwareSerialTx_sendCS(TX_CHAN_INDEX, str);
}

void SIM800_sendStringCSS (
    const CharStringSpan_t* str)
{
    SoftwareSerialTx_sendCSS(TX_CHAN_INDEX, str);
}

void SIM800_sendLineP (
    PGM_P str)
{
    SIM800_sendStringP(str);
    SoftwareSerialTx_sendChar(TX_CHAN_INDEX, 13);
}

void SIM800_sendLineCS (
    const CharString_t* str)
{
    SoftwareSerialTx_sendCS(TX_CHAN_INDEX, str);
    SoftwareSerialTx_sendChar(TX_CHAN_INDEX, 13);
}

void SIM800_sendHex (
    const uint8_t* data)
{
    const uint8_t* dataPtr = data;
    while (*dataPtr != 0) {
        const uint8_t chByte = *dataPtr++;
        SoftwareSerialTx_sendChar(TX_CHAN_INDEX, nibbleHex(chByte >> 4));
        SoftwareSerialTx_sendChar(TX_CHAN_INDEX, nibbleHex(chByte));
    }
}

void SIM800_sendHexP (
    PGM_P data)
{
    PGM_P cp = data;
    uint8_t byte = 0;
    do {
        byte = pgm_read_byte(cp);
        ++cp;
        if (byte != 0) {
            SoftwareSerialTx_sendChar(TX_CHAN_INDEX, nibbleHex(byte >> 4));
            SoftwareSerialTx_sendChar(TX_CHAN_INDEX, nibbleHex(byte));
        }
    } while (byte != 0);
}

void SIM800_sendCtrlZ (void)
{
    SoftwareSerialTx_sendChar(TX_CHAN_INDEX, 26);
}

