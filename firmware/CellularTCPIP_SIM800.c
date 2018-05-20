//
//  Cellular Communications, TCP/IP sesssions for SIM800
//
//

#include "CellularTCPIP_SIM800.h"

#include <stdlib.h>
#include <ctype.h>
#include "SystemTime.h"
#include "SIM800.h"
#include "StringUtils.h"
#include "CellularComm_SIM800.h"
#include "EEPROMStorage.h"
#include "Console.h"

#define CIPACK_BEFORE_CIPSEND 1
#define DEBUG_TRACE 0

// time parameters (in seconds)
#define IPSTATE_REQUEST_DELAY 100
#define CREG_TIMEOUT 500
#define CIPSTART_TIMEOUT 1500
#define SEND_TIMEOUT 1500

// commands
typedef enum CellularTCPIPCommand_enum {
    c_none,
    c_connect,
    c_sendData,
    c_disconnect
} CellularTCPIPCommand;

// states
typedef enum CellularTCPIPState_enum {
    cts_idle,
    cts_waitingForCREGResponse,
    cts_waitingForCGATTResponse,
    cts_waitingForCSTTResponse,
    cts_waitingForCIICRResponse,
    cts_waitingForCIFSRResponse,
    cts_waitingForCIPSTARTResponse,
    cts_waitingForCIPACKResponse,
    cts_waitingForCIPSENDPrompt,
    cts_sendingData,
    cts_waitingForCIPSENDResponse,
    cts_waitingForCIPCLOSEResponse,
    cts_waitingForCIPSHUTResponse,
    cts_waitingForIPState,
    cts_ipstateRequestDelay
} CellularTCPIPState;

// state variables
static CellularTCPIPState ctState = cts_idle;
static SystemTime_t ipstateRequestDelayTime;
CharString_define(32, ctHostAddress);
static uint16_t ctHostPort;
static CellularTCPIP_DataProvider ctDataProvider;
static CellularTCPIP_SendCompletionCallback ctSendCompletionCallback;
static SystemTime_t ctResponseTimeoutTime;
static SIM800_ResponseMessage SIM800ResponseMsg;
static CellularTCPIPCommand curCommand;
static CellularTCPIPConnectionStatus curConnectionStatus;
static CellularTCPIP_ConnectionStateChangeCallback connStateChangeCallback;
static SIM800_IPState curIPState;
static bool gprsIsAttached;
static bool gotIPAddress;
static bool gotPrompt;
static bool gotDataAccept;

void responseMessageCallback (
    const SIM800_ResponseMessage msg)
{
    SIM800ResponseMsg = msg;
    if (SIM800ResponseMsg == rm_CLOSED) {
        CellularTCPIP_notifyConnectionClosed();
    }
}

static void sendSIM800CommandP (
    PGM_P command,
    const CellularTCPIPState responseWaitState)
{
    SIM800_setResponseMessageCallback(responseMessageCallback);
    SIM800ResponseMsg = rm_noResponseYet;
    ctState = responseWaitState;
    SIM800_sendLineP(command);
}

static void sendSIM800CommandCS (
    const CharString_t *command,
    const CellularTCPIPState responseWaitState)
{
    SIM800_setResponseMessageCallback(responseMessageCallback);
    SIM800ResponseMsg = rm_noResponseYet;
    ctState = responseWaitState;
    SIM800_sendLineCS(command);
}

static void resetSubtask (void)
{
    ctState = cts_idle;
    curCommand = c_none;
}

static void setConnectionStatus (
    const CellularTCPIPConnectionStatus newStatus)
{
    curConnectionStatus = newStatus;
    if (connStateChangeCallback != 0) {
        connStateChangeCallback(newStatus);
    }
}

static void endSubtask (
    const CellularTCPIPConnectionStatus connStatus)
{
    if ((curCommand == c_sendData) && (ctSendCompletionCallback != 0)) {
        ctSendCompletionCallback(false);
    }
    setConnectionStatus(connStatus);
    if (connStatus == cs_disconnected) {
        connStateChangeCallback = 0;
    }
    resetSubtask();
}

static void sendCIPSTART (
    CharString_t *cmdBuffer)
{
    CharString_copyP(PSTR("AT+CIPSTART=\"TCP\",\""), cmdBuffer);
    CharString_appendCS(&ctHostAddress, cmdBuffer);
    CharString_appendP(PSTR("\",\""), cmdBuffer);
    StringUtils_appendDecimal(ctHostPort, 1, 0, cmdBuffer);
    CharString_appendP(PSTR("\""), cmdBuffer);
    SystemTime_futureTime(CIPSTART_TIMEOUT, &ctResponseTimeoutTime);
    sendSIM800CommandCS(cmdBuffer, cts_waitingForCIPSTARTResponse);
}

static void CGATTCallback (
    const bool gprsAttached)
{
    gprsIsAttached = gprsAttached;
}

static void requestCGATTStatus (void)
{
    SIM800_setCGATTCallback(CGATTCallback);
    sendSIM800CommandP(PSTR("AT+CGATT?"), cts_waitingForCGATTResponse);
}

static void IPAddressCallback (
    const CharString_t *ipAddress)
{
    gotIPAddress = true;
}

static void requestIPAddress (void)
{
    SIM800_setIPAddressCallback(IPAddressCallback);
    gotIPAddress = false;
    sendSIM800CommandP(PSTR("AT+CIFSR"), cts_waitingForCIFSRResponse);
}

static void IPStateCallback (
    const SIM800_IPState ipState)
{
    curIPState = ipState;
}

static void requestIPState (void)
{
    SIM800_setIPStateCallback(IPStateCallback);
    curIPState = ips_unknown;
    sendSIM800CommandP(PSTR("AT+CIPSTATUS"), cts_waitingForIPState);
}

static void waitBeforeRequestingIPState (void)
{
    SystemTime_futureTime(IPSTATE_REQUEST_DELAY, &ipstateRequestDelayTime);
    ctState = cts_ipstateRequestDelay;
}

static void sendCSTT (
    CharString_t *cmdBuffer)
{
    CharString_copyP(PSTR("AT+CSTT=\""), cmdBuffer);
    EEPROMStorage_getAPN(cmdBuffer);
    CharString_appendC('"', cmdBuffer);
    sendSIM800CommandCS(cmdBuffer, cts_waitingForCSTTResponse);
}

static void promptCallback (
    void)
{
    gotPrompt = true;
}

static void dataAcceptCallback (
    const uint16_t dataSent)
{
    gotDataAccept = true;
}
static void sendCIPSEND (void)
{
    gotPrompt = false;
    SIM800_setPromptCallback(promptCallback);
    gotDataAccept = false;
    SIM800_setDataAcceptCallback(dataAcceptCallback);
    sendSIM800CommandP(PSTR("AT+CIPSEND"), cts_waitingForCIPSENDPrompt);
}

static void sendCIPACK (void)
{
    sendSIM800CommandP(PSTR("AT+CIPACK"), cts_waitingForCIPACKResponse);
}

static void advanceStateForConnect (
    CharString_t *cmdBuffer)
{
    switch (curIPState) {
        case ips_CONNECT_OK :
        case ips_SERVER_LISTENING :
            // successfully connected
            endSubtask(cs_connected);
            break;
        case ips_IP_CONFIG :
            waitBeforeRequestingIPState();
            break;
        case ips_IP_GPRSACT :
            requestIPAddress();
            break;
        case ips_IP_INITIAL :
            sendCSTT(cmdBuffer);
            break;
        case ips_IP_START :
            sendSIM800CommandP(PSTR("AT+CIICR"), cts_waitingForCIICRResponse);
            break;
        case ips_IP_STATUS :
        case ips_TCP_CLOSED :
        case ips_UDP_CLOSED :
            sendCIPSTART(cmdBuffer);
            break;
        default:
            // not expected to ever get here
#if DEBUG_TRACE
            Console_printP(PSTR("unexpected IP state in connect"));
#endif
            resetSubtask();
            break;
    }
}

static void advanceStateForSendData (void)
{
    switch (curIPState) {
        case ips_CONNECT_OK :
        case ips_SERVER_LISTENING :
#if CIPACK_BEFORE_CIPSEND
            sendCIPACK();
#else
            sendCIPSEND();
#endif
            break;
        case ips_IP_CONFIG :
        case ips_IP_GPRSACT :
        case ips_IP_INITIAL :
        case ips_IP_START :
        case ips_IP_STATUS :
        case ips_TCP_CLOSED :
        case ips_UDP_CLOSED :
            endSubtask(cs_disconnected);
            break;
        default:
            // not expected to ever get here
#if DEBUG_TRACE
            Console_printP(PSTR("unexpected IP state in sendData"));
#endif
            resetSubtask();
            break;
    }
}

static void advanceStateForDisconnect (void)
{
    switch (curIPState) {
        case ips_CONNECT_OK :
        case ips_SERVER_LISTENING :
            sendSIM800CommandP(PSTR("AT+CIPCLOSE"), cts_waitingForCIPCLOSEResponse);
            break;
        case ips_IP_CONFIG :
        case ips_IP_GPRSACT :
        case ips_IP_INITIAL :
        case ips_IP_START :
        case ips_IP_STATUS :
        case ips_TCP_CLOSED :
        case ips_UDP_CLOSED :
            endSubtask(cs_disconnected);
            break;
        default:
            // not expected to ever get here
#if DEBUG_TRACE
            Console_printP(PSTR("unexpected IP state in disconnect"));
#endif
            resetSubtask();
            break;
    }
}
static void advanceStateForCommand (
    CharString_t *cmdBuffer)
{
    switch (curIPState) {
        case ips_PDP_DEACT :
            sendSIM800CommandP(PSTR("AT+CIPSHUT"), cts_waitingForCIPSHUTResponse);
            break;
        case ips_TCP_CLOSING :
        case ips_UDP_CLOSING :
            waitBeforeRequestingIPState();
            break;
        case ips_TCP_CONNECTING :
        case ips_UDP_CONNECTING :
            waitBeforeRequestingIPState();
            break;
        case ips_unknown :
            requestIPState();
            break;
        default:
            switch (curCommand) {
                case c_connect :
                    advanceStateForConnect(cmdBuffer);
                    break;
                case c_sendData :
                    advanceStateForSendData();
                    break;
                case c_disconnect :
                    advanceStateForDisconnect();
                    break;
                default :
                    break;
            }
            break;
    }
}

void CellularTCPIP_Initialize (void)
{
    CharString_clear(&ctHostAddress);
    ctHostPort = 0;
    ctDataProvider = 0;
    ctSendCompletionCallback = 0;
    SIM800ResponseMsg = rm_noResponseYet;
    curConnectionStatus = cs_disconnected;
    connStateChangeCallback = 0;
    curIPState = ips_unknown;
    gprsIsAttached = false;
    gotIPAddress = false;
    resetSubtask();
    SIM800_setPDPDeactCallback(CellularTCPIP_notifyConnectionClosed);
}

CellularTCPIPConnectionStatus CellularTCPIP_connectionStatus (void)
{
    return curConnectionStatus;
}

void CellularTCPIP_connect (
    const CharString_t *hostAddress,
    const uint16_t hostPort,
    SIM800_IPDataCallback receiver,
    CellularTCPIP_ConnectionStateChangeCallback stateChangeCallback)
{
    CharString_copyCS(hostAddress, &ctHostAddress);
    ctHostPort = hostPort;
    SIM800_setIPDataCallback(receiver);
    connStateChangeCallback = stateChangeCallback;
    curCommand = c_connect;
}

void CellularTCPIP_sendData (
    CellularTCPIP_DataProvider provider,
    CellularTCPIP_SendCompletionCallback completionCallback)
{
    if (provider != NULL) {
        ctDataProvider = provider;
        ctSendCompletionCallback = completionCallback;

        curCommand = c_sendData;
    }
}

void CellularTCPIP_disconnect (void)
{
    curCommand = c_disconnect;
}

void CellularTCPIP_notifyConnectionClosed (void)
{
    if (ctState == cts_idle) {
        endSubtask(cs_disconnected);
    }
}

bool CellularTCPIP_hasSubtaskWorkToDo (void)
{
    return ((ctState != cts_idle) || (curCommand != c_none));
}

void CellularTCPIP_Subtask (void)
{
    CharString_define(70, cmdBuffer);// define here to keep stack frame small
    switch (ctState) {
        case cts_idle :
            switch (curConnectionStatus) {
                case cs_connected :
                    if (curCommand == c_sendData) {
                        // send data
#if DEBUG_TRACE
                        Console_printP(PSTR("sending data"));
#endif
                        setConnectionStatus(cs_sendingData);
                        requestIPState();
                    } else  if (curCommand == c_disconnect) {
                        // close connection
#if DEBUG_TRACE
                        Console_printP(PSTR("disconnecting TCPIP"));
#endif
                        setConnectionStatus(cs_disconnecting);
                        requestIPState();
                    }
                    break;
                case cs_disconnected :
                    if (curCommand == c_connect) {
#if DEBUG_TRACE
                        Console_printP(PSTR("connecting TCPIP"));
#endif
                        setConnectionStatus(cs_connecting);

                        // begin by checking registration
                        CellularComm_requestRegistrationStatus();
                        SystemTime_futureTime(CREG_TIMEOUT, &ctResponseTimeoutTime);
                        ctState = cts_waitingForCREGResponse;
                    }
                    break;
                default :
#if DEBUG_TRACE
                    Console_printP(PSTR("unexpected connection status"));
#endif
                    // we don't expect to ever get here - for any other
                    // connection status we won't be in the idle state
                    break;
            }
            break;
        case cts_waitingForCREGResponse :
            if (SystemTime_timeHasArrived(&ctResponseTimeoutTime)) {
                endSubtask(cs_disconnected);
            } else if (CellularComm_gotRegistrationStatus()) {
                if (CellularComm_isRegistered()) {
                    // registered
                    requestCGATTStatus();
                } else {
                    Console_printP(PSTR("not registered"));
                    CharString_clear(&ctHostAddress);

                    endSubtask(cs_disconnected);
                }
            }
            break;
        case cts_waitingForCGATTResponse :
            if (SIM800ResponseMsg == rm_OK) {
                if (gprsIsAttached) {
                    // GPRS is attached - check ipstatus
                    requestIPState();
                } else {
                    // no GPRS - cannot proceed
                    Console_printP(PSTR("no GPRS"));
                    endSubtask(cs_disconnected);
                }
            } else if (SIM800ResponseMsg == rm_ERROR) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_waitingForCSTTResponse :
            if (SIM800ResponseMsg == rm_OK) {
                requestIPState();
            } else if (SIM800ResponseMsg == rm_ERROR) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_waitingForCIICRResponse :
            if (SIM800ResponseMsg == rm_OK) {
                requestIPState();
            } else if (SIM800ResponseMsg == rm_ERROR) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_waitingForCIFSRResponse :
            if (gotIPAddress) {
                requestIPState();
            } else if (SIM800ResponseMsg == rm_ERROR) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_waitingForCIPSTARTResponse :
            if (SystemTime_timeHasArrived(&ctResponseTimeoutTime)) {
                endSubtask(cs_disconnected);
            } else {
                switch (SIM800ResponseMsg) {
                    case rm_CONNECT_OK :
                        endSubtask(cs_connected);
                        break;
                    case rm_ERROR :
                    case rm_CONNECT_FAIL :
                        endSubtask(cs_disconnected);
                        break;
                    default:
                        break;
                }
            }
            break;
        case cts_waitingForCIPSENDPrompt :
            if (gotPrompt) {
                ctState = cts_sendingData;
            } else if ((SIM800ResponseMsg == rm_ERROR) ||
                       (SIM800ResponseMsg == rm_CLOSED)) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_sendingData :
            if (ctDataProvider()) {
                // completed providing data
                SIM800_sendCtrlZ();
                Console_printP(PSTR("sent Ctrl-Z"));
                SystemTime_futureTime(SEND_TIMEOUT, &ctResponseTimeoutTime);
                ctState = cts_waitingForCIPSENDResponse;
            }
            break;
        case cts_waitingForCIPSENDResponse : {
            bool sendComplete = false;
            bool sendSuccessful = false;
            bool timedOut = false;
            if ((SIM800ResponseMsg == rm_SEND_OK) || gotDataAccept) {
                sendComplete = true;
                sendSuccessful = true;
            } else if ((SIM800ResponseMsg == rm_SEND_FAIL) ||
                       (SIM800ResponseMsg == rm_ERROR) ||
                       (SIM800ResponseMsg == rm_CLOSED)) {
                sendComplete = true;
            } else if (SystemTime_timeHasArrived(&ctResponseTimeoutTime)) {
                sendComplete = true;
                timedOut = true;
            }
            if (sendComplete) {
                if (ctSendCompletionCallback != 0) {
                    ctSendCompletionCallback(sendSuccessful);
                    ctSendCompletionCallback = 0;
                }
                endSubtask(
                    (sendSuccessful)
                    ? cs_connected
                    : cs_disconnected);
            }
            }
            break;
        case cts_waitingForCIPACKResponse :
            if (SIM800ResponseMsg == rm_OK) {
                sendCIPSEND();
            } else if ((SIM800ResponseMsg == rm_ERROR) ||
                       (SIM800ResponseMsg == rm_CLOSED)) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_waitingForCIPCLOSEResponse :
            if ((SIM800ResponseMsg == rm_CLOSE_OK) ||
                (SIM800ResponseMsg == rm_ERROR) ||
                (SIM800ResponseMsg == rm_CLOSED)) {
                endSubtask(cs_disconnected);
            }
            break;
        case cts_waitingForCIPSHUTResponse :
            switch (SIM800ResponseMsg) {
                case rm_SHUT_OK :
                    requestIPState();
                    break;
                case rm_ERROR :
                case rm_CLOSED :
                    endSubtask(cs_disconnected);
                    break;
                default:
                    break;
            }
            break;
        case cts_waitingForIPState :
            if (curIPState != ips_unknown) {
                // we got a state
                advanceStateForCommand(&cmdBuffer);
            } else if ((SIM800ResponseMsg == rm_ERROR)  ||
                       (SIM800ResponseMsg == rm_CLOSED)) {
                endSubtask(cs_disconnected);
            }
            // may want to have a time out here
            break;
        case cts_ipstateRequestDelay :
            if (SystemTime_timeHasArrived(&ipstateRequestDelayTime)) {
                requestIPState();
            }
            break;
        default:
            // unexpected state
            // trigger system reset here
            SystemTime_commenceShutdown();
            break;
    }
}

int CellularTCPIP_state (void)
{
    return (int)ctState;
}

uint16_t CellularTCPIP_availableSpaceForWriteData (void)
{
    return SIM800_availableSpaceForSend();
}

void CellularTCPIP_writeDataP (
    PGM_P data)
{
    SIM800_sendStringP(data);
}

void CellularTCPIP_writeDataCS (
    CharString_t *data)
{
    SIM800_sendStringCS(data);
}

void CellularTCPIP_writeDataCSS (
    CharStringSpan_t *data)
{
    SIM800_sendStringCSS(data);
}
