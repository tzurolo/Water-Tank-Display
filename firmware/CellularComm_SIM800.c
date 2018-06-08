//
//  Cellular Communications using SIM800 Quad-band GSM/GPRS module
//
//  I/O Pin Usage
//      D4 - US/International PIN jumper
//
#include "CellularComm_SIM800.h"

#include "SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "SystemTime.h"
#include "MessageIDQueue.h"
#include "CommandProcessor.h"
#include "SoftwareSerialRx0.h"
#include "TCPIPConsole.h"
#include "Console.h"
#include "EEPROMStorage.h"
#include "StringUtils.h"
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define CONSULT_PINJUMPER 0
#define USE_CMGL 0
#define DEBUG_TRACE 0

#define PINJUMPER_PIN       PD4
#define PINJUMPER_INPORT    PIND
#define PINJUMPER_OUTPORT   PORTD

// if the state machine is in the same state for more than
// this many hundredths of a second (such as if the cell module freezes up)
// the system will commence shutdown and reboot
#define STATE_TIMEOUT_TIME 18000L

typedef enum SMSMessageStatus_enum {
    sms_unknown,
    sms_recUnread,
    sms_recRead,
    sms_stoUnsent,
    sms_stoSent,
    sms_all
} SMSMessageStatus;
char smsStatusUndefined[]   PROGMEM = "";
char smsStatusRecUnread[]   PROGMEM = "REC UNREAD";
char smsStatusRecRead[]     PROGMEM = "REC READ";
char smsStatusStoUnsent[]   PROGMEM = "STO UNSENT";
char smsStatusStoSent[]     PROGMEM = "STO SENT";
char smsStatusAll[]         PROGMEM = "ALL";

PGM_P smsStatusStrings[] PROGMEM = 
{
    smsStatusUndefined,
    smsStatusRecUnread,
    smsStatusRecRead,
    smsStatusStoUnsent,
    smsStatusStoSent,
    smsStatusAll
};

char smsCommandPrefix[]   PROGMEM = "ExeCmd:";

// states
typedef enum CellularCommState_enum {
    ccs_initial,
    ccs_disabling,
    ccs_waitingForSIM800PowerDown,
    ccs_disabled,
    ccs_idle,
    ccs_waitingForOnkeyResponse,
    ccs_lettingADH8066Initialize,
    ccs_waitingForEchoOffResponse,
    ccs_waitingForCMGFResponse,
    ccs_waitingForCIPHEADResponse,
    ccs_waitingForInitialCBCResponse,
    ccs_waitingForInitialCSQResponse,
    ccs_waitingForCIPQSENDResponse,
    ccs_waitingForCPINQueryResponse,
    ccs_waitingForCPINEntryResponse,
    ccs_waitingForCREGResponse,
    ccs_waitToRecheckCREG,
    ccs_waitingForCMGLResponse,
    ccs_getCMGLMessageText,
    ccs_waitingForCMGRResponse,
    ccs_getCMGRMessageText,
    ccs_waitingForCMGROK,
    ccs_waitingForCMGDResponse,
    ccs_waitingForCMGSPrompt,
    ccs_waitingForCMGSResponse,
    ccs_waitingForCCLKResponse,
    ccs_waitingForCSQResponse,
    ccs_waitingForCBCResponse,
    ccs_runningTCPIPSubtask
} CellularCommState;
// state variables
static bool ccEnabled;
static CellularCommState ccState = ccs_initial;
static CellularCommState prevCcState = ccs_initial;
static SystemTime_t stateTimeoutTime;

static SystemTime_t powerupResumeTime;
static bool needToEnterPIN;

// variables for incoming SMS messages
static SystemTime_t nextCheckForIncomingSMSMessageTime;
static SMSMessageStatus CMGLSMSMessageStatus;
static SMSMessageStatus incomingSMSMessageStatus;
CharString_define(16, incomingSMSMessagePhoneNumber)
CharString_define(80, incomingSMSMessageText)
static MessageIDQueue_type incomingSMSMessageIDs;
static MessageIDQueue_ValueType currentlyProcessingMessageID;
static bool gotCMGRMessage;

// variables for outgoing SMS messages
CharString_define(16, outgoingSMSMessagePhoneNumber)
CharString_define(80, outgoingSMSMessageText)

// variables for cell registration, network time, etc
static SystemTime_t nextCheckForNetworkTimeAndCSQTime;
static bool gotFunctionlevel;
static uint8_t functionlevel;
static bool gotNetworkTime;
static SIM800_NetworkTime currentNetworkTime;
static bool gotCREG;
static uint8_t cregStatus;
static bool gotSignalQuality;
static uint8_t signalQuality;
static bool gotCPIN;
CharString_define(10, cpinStatus);
static uint8_t batteryChargeStatus;
static uint8_t batteryPercent;
static uint16_t batteryMillivolts;

// variables for responses from SIM800
static SIM800_ResponseMessage SIM800ResponseMsg;
static bool gotSIM800Prompt;

PGM_P imageOfMessageStatus (
    const SMSMessageStatus status)
{
    return (PGM_P)pgm_read_word(&(smsStatusStrings[status]));
}

SMSMessageStatus messageStatusFromString (
    const CharString_t *statusString)
{
    CharStringSpan_t statusStringSpan;
    CharStringSpan_init(statusString, &statusStringSpan);
    SMSMessageStatus msgStatus = sms_unknown;
    for (msgStatus = sms_all; msgStatus > sms_unknown;
            msgStatus = (SMSMessageStatus)(((int)msgStatus) - 1)) {
        if (CharStringSpan_equalsNocaseP(&statusStringSpan, imageOfMessageStatus(msgStatus))) {
            break;
        }
    }

    return msgStatus;
}

//
// This section provides handling of responses from
// the SIM800
//
static void responseCallback (
    const SIM800_ResponseMessage msg)
{
    SIM800ResponseMsg = msg;
    if (SIM800ResponseMsg == rm_CLOSED) {
        CellularTCPIP_notifyConnectionClosed();
    }
}

#if DEBUG_TRACE
static void printQueueingMessage (
    const int16_t msgID)
{
    CharString_define(20, msg);
    CharString_copyP(PSTR("queueing msg "), &msg);
    StringUtils_appendDecimal(msgID, 1, 0, &msg);
    Console_printCS(&msg);
}
#endif

#if USE_CMGL
static void SMSCMGLCallback (
    const int16_t msgID,
    const CharString_t *messageStatus, 
    const CharString_t *phoneNumber, 
    const CharString_t *message)
{
    // only interested in message IDs. will read with CMGR
    if (!MessageIDQueue_isFull(&incomingSMSMessageIDs)) {
        printQueueingMessage(msgID);
        MessageIDQueue_insert(msgID, &incomingSMSMessageIDs);
    }
}
#endif

static void SMSCMGRCallback (
    const int16_t msgID,
    const CharString_t *messageStatus, 
    const CharString_t *phoneNumber, 
    const CharString_t *message)
{
    gotCMGRMessage = true;
    incomingSMSMessageStatus = messageStatusFromString(messageStatus);
    CharString_copyCS(phoneNumber, &incomingSMSMessagePhoneNumber);
    CharString_copyCS(message, &incomingSMSMessageText);
}

static void CFUNCallback (
    const int16_t funclevel)
{
    gotFunctionlevel = true;
    functionlevel = funclevel;
}

static void CSQCallback (
    const int16_t sigStrength)
{
    gotSignalQuality = true;
    signalQuality = sigStrength;
}

static void CREGCallback (
    const int16_t reg)
{
    gotCREG = true;
    cregStatus = reg;
}

static void CMTICallback (
    const int16_t msgID)
{
    if (!MessageIDQueue_isFull(&incomingSMSMessageIDs)) {
#if DEBUG_TRACE
        printQueueingMessage(msgID);
#endif
        MessageIDQueue_insert(msgID, &incomingSMSMessageIDs);
    }
}


static void CPINCallback (
    const CharStringSpan_t *status)
{
    gotCPIN = true;
    CharString_copyIters(
        CharStringSpan_begin(status),
        CharStringSpan_end(status),
        &cpinStatus);
}

static void CCLKCallback (
    const SIM800_NetworkTime *time)
{
    gotNetworkTime = true;
    currentNetworkTime = *time;
}

static void CBCCallback (
    const uint8_t chargeStatus,
    const uint8_t percent,
    const uint16_t millivolts)
{
    batteryChargeStatus = chargeStatus;
    batteryPercent = percent;
    batteryMillivolts = millivolts;
}

static void promptCallback (
    void)
{
    gotSIM800Prompt = true;
}

static void sendSIM800CommandP (
    PGM_P command)
{
    SIM800_setResponseMessageCallback(responseCallback);
    SIM800ResponseMsg = rm_noResponseYet;
    SIM800_sendLineP(command);
}

static void sendSIM800CommandCS (
    const CharString_t *command)
{
    SIM800_setResponseMessageCallback(responseCallback);
    SIM800ResponseMsg = rm_noResponseYet;
    SIM800_sendLineCS(command);
}

#if USE_CMGL
static void sendCMGLCommand (
    const SMSMessageStatus requestedStatus)
{
    CharString_clear(&incomingSMSMessagePhoneNumber);
    CharString_clear(&incomingSMSMessageText);
    SIM800_setSMSMessageReceivedCallback(SMSCMGLCallback);

    CharString_define(20, command);
    CharString_copyP(PSTR("AT+CMGL=\""), &command);
    CharString_appendP(imageOfMessageStatus(requestedStatus), &command);
    CharString_appendC('"', &command);
    sendSIM800CommandCS(&command);
}
#endif

static void sendCMGRCommand (
    const int16_t msgID)
{
    gotCMGRMessage = false;
    CharString_clear(&incomingSMSMessageText);
    SIM800_setSMSMessageReceivedCallback(SMSCMGRCallback);

    CharString_define(10, command);
    CharString_copyP(PSTR("AT+CMGR="), &command);
    StringUtils_appendDecimal(msgID, 1, 0, &command);
    sendSIM800CommandCS(&command);
}

static void sendCMGDCommand (
    const int16_t msgID)
{
    CharString_define(10, command);
    CharString_copyP(PSTR("AT+CMGD="), &command);
    StringUtils_appendDecimal(msgID, 1, 0, &command);
    sendSIM800CommandCS(&command);
}

static void sendCMGSCommand (
    const CharString_t *phoneNumber)
{
    gotSIM800Prompt = false;
    SIM800_setPromptCallback(promptCallback);

    CharString_define(50, command);
    CharString_copyP(PSTR("AT+CMGS=\""), &command);
    CharString_appendCS(phoneNumber, &command);
    CharString_appendC('"', &command);
    sendSIM800CommandCS(&command);
}

static void sendCSQCommand (void)
{
    gotSignalQuality = false;
    sendSIM800CommandP(PSTR("AT+CSQ"));
}

static void sendCCLKCommand (void)
{
    gotNetworkTime = false;
    sendSIM800CommandP(PSTR("AT+CCLK?"));
}

static void sendCBCCommand (void)
{
    sendSIM800CommandP(PSTR("AT+CBC"));
}

// power down cellular module
static void powerDownCellularModule (void)
{
    SIM800_powerOff();
    cregStatus = 0;
    signalQuality = 0;
}

#if CONSULT_PINJUMPER
static bool responseIsNeedPIN (void)
{
    return CharString_equalsP(&ADH8066Response, PSTR("+CPIN: SIM PIN"));
}
#endif

//
// CellularCom member functions
//

void CellularComm_Initialize (void)
{
#if CONSULT_PINJUMPER
    // enable pull-up on US/International PIN selection jumper
    PINJUMPER_OUTPORT |= (1 << PINJUMPER_PIN);
#endif

    SIM800_Initialize(SoftwareSerial_rx0Queue(), ps_b, 5);
    CMGLSMSMessageStatus = sms_all; // initially clean out all messages

    // setup SIM800 response / prompt callbacks
    SIM800_setCFUNCallback(CFUNCallback);
    SIM800_setCSQCallback(CSQCallback);
    SIM800_setCMTICallback(CMTICallback);
    SIM800_setCPINCallback(CPINCallback);
    SIM800_setCCLKCallback(CCLKCallback);
    SIM800_setCBCCallback(CBCCallback);

    CellularTCPIP_Initialize();

    // set up for incoming SMS messages
    MessageIDQueue_init(&incomingSMSMessageIDs);

    // set up for outgoing SMS messages
    CharString_clear(&outgoingSMSMessagePhoneNumber);
    CharString_clear(&outgoingSMSMessageText);

    ccEnabled = false;
    gotFunctionlevel = false;
    functionlevel = 0;
    gotNetworkTime = false;
    cregStatus = 0;
    gotSignalQuality = false;
    signalQuality = 0;
    ccState = ccs_initial;
    SystemTime_futureTime(STATE_TIMEOUT_TIME, &stateTimeoutTime);
    gotCREG = false;
    gotCPIN = false;
    batteryChargeStatus = 0;
    batteryPercent = 0;
    batteryMillivolts = 0;
}

void CellularComm_Enable (void)
{
    ccEnabled = true;
}

void CellularComm_Disable (void)
{
    ccEnabled = false;
}

void CellularComm_task (void)
{
    // state timeout logic. reboots if stuck in a state
    if ((ccState == prevCcState) && (ccState != ccs_disabled)) {
        if ((!SystemTime_shuttingDown()) &&
            (SystemTime_timeHasArrived(&stateTimeoutTime))) {
            Console_printP(PSTR("!! Timeout !!"));
            EEPROMStorage_setTimeoutState((int)ccState);
            SystemTime_commenceShutdown();
        }
    } else {
        SystemTime_futureTime(STATE_TIMEOUT_TIME, &stateTimeoutTime);
        prevCcState = ccState;
    }

    switch (ccState) {
        case ccs_initial :
            if (ccEnabled) {
                if (SIM800_status() == SIM800_ms_off) {
                    Console_printP(PSTR("> Enabling Cell <"));
                    SIM800_powerOn();
                    ccState = ccs_waitingForOnkeyResponse;
                } else {
                    // already on
                    ccState = ccs_idle;
                }
            } else {
                ccState = ccs_disabled;
            }
            break;
        case ccs_disabling :
            // wait until tcpip disconnects
            CellularTCPIP_Subtask();
            if (CellularTCPIP_connectionStatus() == cs_disconnected) {
                powerDownCellularModule();
                ccState = ccs_waitingForSIM800PowerDown;
            }
            break;
        case ccs_waitingForSIM800PowerDown :
            if (SIM800_status() == SIM800_ms_off) {
                ccState = ccs_disabled;
            }
            break;
        case ccs_disabled :
            if (ccEnabled) {
                // exit the disabled state
                // TCPIPConsole_restoreEnablement();
                ccState = ccs_initial;
            }
            break;
        case ccs_idle : {
            if (ccEnabled) {
                if (CellularComm_isRegistered()) {
                    // check for queued message ids, notifications to send,
                    // or regularly scheduled functions that are due to run
                    if (!CharString_isEmpty(&outgoingSMSMessagePhoneNumber)) {
                        // we have an outgoing message to send
#if DEBUG_TRACE
                        CharString_define(80, cmgs);
                        Console_printP(PSTR("sending outgoing msg"));
                        CharString_copyP(PSTR("SMS send \""), &cmgs);
                        CharString_appendCS(
                            &outgoingSMSMessagePhoneNumber, &cmgs);
                        CharString_appendC('"', &cmgs);
                        Console_printCS(&cmgs);
#endif
                        sendCMGSCommand(&outgoingSMSMessagePhoneNumber);
                        CharString_clear(&outgoingSMSMessagePhoneNumber);
                        ccState = ccs_waitingForCMGSPrompt;
                    } else if (!MessageIDQueue_isEmpty(&incomingSMSMessageIDs)) {
                        // we have an incoming message ID
                        currentlyProcessingMessageID =
                            MessageIDQueue_remove(&incomingSMSMessageIDs);
                        // read the message
#if DEBUG_TRACE
                        CharString_define(30, cmgr);
                        CharString_copyP(PSTR("processing message ID "), &cmgr);
                        StringUtils_appendDecimal32(
                            currentlyProcessingMessageID, 1, 0, &cmgr); 
                        Console_printCS(&cmgr);
#endif
                        sendCMGRCommand(currentlyProcessingMessageID);
                        ccState = ccs_waitingForCMGRResponse;
#if USE_CMGL
                    } else if (SystemTime_timeHasArrived(&nextCheckForIncomingSMSMessageTime)) {
                        Console_printP(PSTR("Check for incoming SMS"));
                        sendCMGLCommand(CMGLSMSMessageStatus);
                        ccState = ccs_waitingForCMGLResponse;
#endif
                    } else if (CellularTCPIP_hasSubtaskWorkToDo()) {
                        ccState = ccs_runningTCPIPSubtask;
                    } else if (SystemTime_timeHasArrived(&nextCheckForNetworkTimeAndCSQTime)) {
#if DEBUG_TRACE
                        Console_printP(PSTR("Check time"));
#endif
                        sendCCLKCommand();
                        ccState = ccs_waitingForCCLKResponse;
                    }
                } else {
                    CellularComm_requestRegistrationStatus();
                    ccState = ccs_waitingForCREGResponse;
                }
            } else {
                // cellular com is disabled. enter disabled state
                TCPIPConsole_disable(false);
                CellularTCPIP_disconnect();
                ccState = ccs_disabling;
                Console_printP(PSTR("> Disabling Cell <"));
            }
            }
            break;
        case ccs_waitingForOnkeyResponse : {
            if (SIM800_status() == SIM800_ms_readyForCommand) {
                Console_printP(PSTR("> Cell Ready <"));

                // turn echo off
                sendSIM800CommandP(PSTR("ATE0"));
                ccState = ccs_waitingForEchoOffResponse;
            }
            }
            break;
        case ccs_waitingForEchoOffResponse : {
            if (SIM800ResponseMsg == rm_OK) {
#if DEBUG_TRACE
                Console_printP(PSTR("Checking PIN"));
#endif
                // SIM800 sends CPIN report unsolicited
                // sendSIM800CommandP(PSTR("AT+CPIN?"));
                gotCPIN = false;
                ccState = ccs_waitingForCPINQueryResponse;
                needToEnterPIN = false;
            }
            }
            break;
        case ccs_waitingForCPINQueryResponse : {
            if (gotCPIN) {
                Console_printP(PSTR("PIN ready."));

                // set SMS to text mode
                sendSIM800CommandP(PSTR("AT+CMGF=1"));
                ccState = ccs_waitingForCMGFResponse;
            }
            }
            break;
        case ccs_waitingForCMGFResponse :
            if (SIM800ResponseMsg == rm_OK) {
                // enable or disable quick send
                CharString_define(16, cipqsend);
                CharString_copyP(PSTR("AT+CIPQSEND="), &cipqsend);
                CharString_appendC((EEPROMStorage_cipqsend() != 0) ? '1' : '0', &cipqsend);
                sendSIM800CommandCS(&cipqsend);
                ccState = ccs_waitingForCIPQSENDResponse;
            }
            break;
        case ccs_waitingForCIPQSENDResponse :
            if (SIM800ResponseMsg == rm_OK) {
                // enable IP header for received data to show data length
                sendSIM800CommandP(PSTR("AT+CIPHEAD=1"));
                ccState = ccs_waitingForCIPHEADResponse;
            }
            break;
        case ccs_waitingForCIPHEADResponse :
            if (SIM800ResponseMsg == rm_OK) {
                sendCBCCommand();
                ccState = ccs_waitingForInitialCBCResponse;
            }
        case ccs_waitingForInitialCBCResponse :
            if (SIM800ResponseMsg == rm_OK) {
                CellularComm_requestRegistrationStatus();
                ccState = ccs_waitingForCREGResponse;
            }
            break;
        case ccs_waitingForCREGResponse : {
            if (CellularComm_gotRegistrationStatus()) {
                if (CellularComm_isRegistered()) {
                    Console_printP(PSTR("Registered."));

#if USE_CMGL
                    SystemTime_futureTime(500, &nextCheckForIncomingSMSMessageTime);
#endif
                    SystemTime_futureTime(1000, &nextCheckForNetworkTimeAndCSQTime);
                    sendCSQCommand();
                    ccState = ccs_waitingForInitialCSQResponse;
                } else {
                    // not registered yet.
                    ccState = ccs_waitToRecheckCREG;
                    SystemTime_futureTime(200, &powerupResumeTime);
                }
            }
            }
            break;
        case ccs_waitToRecheckCREG : {
            if (SystemTime_timeHasArrived(&powerupResumeTime)) {
                CellularComm_requestRegistrationStatus();
                ccState = ccs_waitingForCREGResponse;
            }
            }
            break;
        case ccs_waitingForInitialCSQResponse :
            if (SIM800ResponseMsg == rm_OK) {
                ccState = ccs_idle;
            }
            break;
#if USE_CMGL
        case ccs_waitingForCMGLResponse : {
            if (SIM800ResponseMsg == rm_OK) {
                if (MessageIDQueue_isEmpty(&incomingSMSMessageIDs)) {
                    // no more messages in SIM card. we only need to
                    // check for unread received messages from now on
                    CMGLSMSMessageStatus = sms_recUnread;
                }
                // schedule next check
                SystemTime_futureTime(200, &nextCheckForIncomingSMSMessageTime);
                ccState = ccs_idle;
            }
            }
            break;
#endif
        case ccs_waitingForCMGRResponse : {
            if (SIM800ResponseMsg == rm_OK) {
                // we now have a complete message
                // we will process the message text after
                // the message has been deleted from
                // the cell module
                Console_printP(PSTR("Got CMGR."));
#if DEBUG_TRACE
                Console_printCS(&incomingSMSMessagePhoneNumber);
                Console_printP(imageOfMessageStatus(incomingSMSMessageStatus));
                Console_printCS(&incomingSMSMessageText);
#endif

                // delete the message now
#if DEBUG_TRACE
                Console_printP(PSTR("Deleting message"));
#endif
                sendCMGDCommand(currentlyProcessingMessageID);
                ccState = ccs_waitingForCMGDResponse;
            }
            }
            break;
        case ccs_waitingForCMGDResponse : {
            if (SIM800ResponseMsg == rm_OK) {
#if DEBUG_TRACE
                Console_printP(PSTR("message deleted"));
#endif
                if ((incomingSMSMessageStatus == sms_recUnread) ||
                    (incomingSMSMessageStatus == sms_recRead)) {
                    // process the message we got
                    // check if it's an executable command (i.e. has the
                    // executable command prefix)
                    if (CharString_startsWithP(&incomingSMSMessageText,
                        smsCommandPrefix)) {
                        // we have an executable command
                        CharString_clear(&outgoingSMSMessageText);
                        CharString_clear(&outgoingSMSMessagePhoneNumber);
                        CharStringSpan_t cmd;
                        CharStringSpan_initRight(&incomingSMSMessageText,
                            strlen_P(smsCommandPrefix), &cmd);
                        if (CommandProcessor_executeCommand(&cmd, &outgoingSMSMessageText)) {
                            // valid command
                            if (!CharString_isEmpty(&outgoingSMSMessageText)) {
                                // a reply was generated from the command
                                if (CharString_isEmpty(&outgoingSMSMessagePhoneNumber)) {
                                    // the command did not set the outgoing phone number
                                    // use the incoming phone number
                                    CharString_copyCS(&incomingSMSMessagePhoneNumber,
                                                        &outgoingSMSMessagePhoneNumber);
                                }
                            }
                        }
                    }
                }
                ccState = ccs_idle;
            }
            }
            break;
        case ccs_waitingForCMGSPrompt : {
            if (gotSIM800Prompt) {
                Console_printP(PSTR("sending SMS"));
                SIM800_sendStringCS(&outgoingSMSMessageText);
                CharString_clear(&outgoingSMSMessageText);
                SIM800_sendCtrlZ();
                ccState = ccs_waitingForCMGSResponse;
            }
            }
            break;
        case ccs_waitingForCMGSResponse : {
            if (SIM800ResponseMsg == rm_OK) {
                Console_printP(PSTR("SMS sent"));
                ccState = ccs_idle;
            } else if (SIM800ResponseMsg == rm_ERROR) {
                Console_printP(PSTR("SMS send error"));
                ccState = ccs_idle;
            }
            }
            break;
        case ccs_waitingForCCLKResponse : {
            if (SIM800ResponseMsg == rm_OK) {
                sendCSQCommand();
                ccState = ccs_waitingForCSQResponse;
            }
            }
            break;
        case ccs_waitingForCSQResponse : {
            if (SIM800ResponseMsg == rm_OK) {
                sendCBCCommand();
                ccState = ccs_waitingForCBCResponse;
            }
            }
            break;
        case ccs_waitingForCBCResponse : {
            if (SIM800ResponseMsg == rm_OK) {
                SystemTime_futureTime(6000, &nextCheckForNetworkTimeAndCSQTime);
                ccState = ccs_idle;
            }
            }
            break;
        case ccs_runningTCPIPSubtask : {
            CellularTCPIP_Subtask();
            if (!CellularTCPIP_hasSubtaskWorkToDo()) {
#if DEBUG_TRACE
                Console_printP(PSTR("TCPIP subtask completed"));
#endif
                ccState = ccs_idle;
            } else if (!ccEnabled) {
                CellularTCPIP_disconnect();
            }
            }
            break;
        default:
            // unexpected state
            // trigger system reset here
            SystemTime_commenceShutdown();
            break;
    }
}

bool CellularComm_isEnabled (void)
{
    return (ccState != ccs_disabled);
}

bool CellularComm_isIdle (void)
{
    return ((ccState == ccs_disabled) || (ccState == ccs_idle)) &&
        CharString_isEmpty(&outgoingSMSMessagePhoneNumber) &&
        MessageIDQueue_isEmpty(&incomingSMSMessageIDs) &&
        (!CellularTCPIP_hasSubtaskWorkToDo());
}

void CellularComm_requestRegistrationStatus (void)
{
    SIM800_setCREGCallback(CREGCallback);
    gotCREG = false;
    sendSIM800CommandP(PSTR("AT+CREG?"));
}

bool CellularComm_gotRegistrationStatus (void)
{
    return (gotCREG && (SIM800ResponseMsg == rm_OK));
}

uint8_t CellularComm_registrationStatus (void)
{
    return cregStatus;
}

bool CellularComm_isRegistered (void)
{
    return
        (cregStatus == 1) ||  // registered, home network
        (cregStatus == 5);    // registered, roaming
}

const SIM800_NetworkTime* CellularComm_currentTime (void)
{
    return &currentNetworkTime;
}

uint8_t CellularComm_SignalQuality (void)
{
    return signalQuality;
}

int CellularComm_state (void)
{
    return (int)ccState;
}

bool CellularComm_stateIsTCPIPSubtask (
    const int state)
{
    return (state == (int)ccs_runningTCPIPSubtask);
}

uint16_t CellularComm_batteryMillivolts (void)
{
    return batteryMillivolts;
}

void CellularComm_setOutgoingSMSMessageNumber (
    const CharStringSpan_t* phoneNumber)
{
    CharString_copyIters(
        CharStringSpan_begin(phoneNumber),
        CharStringSpan_end(phoneNumber),
        &outgoingSMSMessagePhoneNumber);
}

