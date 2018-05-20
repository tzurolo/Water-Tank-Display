//
//  TCPIP Console
//
//  Maintains a connection to the host
//

#include "TCPIPConsole.h"
#include "Console.h"
#include "CommandProcessor.h"
#include "SystemTime.h"
#include "EEPROMStorage.h"

#define DEBUG_TRACE 0

typedef enum SendingState_enum {
    ss_idle,
    ss_waitingForTCPIPConnecting,
    ss_waitingForTCPIPConnection,
    ss_waitingForTCPIPSendingData,
    ss_waitingForTCPIPSentData,
    ss_waitingForDisconnect
} SendingState;

static bool isEnabled;
static SIM800_IPDataCallback dataReceiver;
static CellularTCPIP_DataProvider sendDataProvider;
static CellularTCPIP_SendCompletionCallback sendCompletionCallback;
static SendingState sState;
static SystemTime_t nextConnectAttemptTime;

static void statusCallback (
    const CellularTCPIPConnectionStatus status)
{
#if DEBUG_TRACE
    switch (status) {
        case cs_connecting :
            Console_printP(PSTR(">>> Connecting"));
            break;
        case cs_connected :
            Console_printP(PSTR(">>> Connected"));
            break;
        case cs_sendingData :
            Console_printP(PSTR(">>> Sending Data"));
            break;
        case cs_disconnecting :
            Console_printP(PSTR(">>> Disconnecting"));
            break;
        case cs_disconnected :
            Console_printP(PSTR(">>> Disconnected"));
            break;
    }
#endif
}

void TCPIPConsole_Initialize (void)
{
    TCPIPConsole_restoreEnablement();
    dataReceiver = 0;
    sendDataProvider = 0;
    sendCompletionCallback = 0;
    sState = ss_idle;
    // wait 10 seconds before attempting first connection
    SystemTime_futureTime(1000, &nextConnectAttemptTime);
}

void TCPIPConsole_task (void)
{
    switch (sState) {
        case ss_idle : {
            const CellularTCPIPConnectionStatus connStatus = CellularTCPIP_connectionStatus();
            switch (connStatus) {
                case cs_connected :
                    if (isEnabled) {
                        if (sendDataProvider != 0) {
                            CellularTCPIP_sendData(sendDataProvider, sendCompletionCallback);
                            sState = ss_waitingForTCPIPSendingData;
                        }
                    } else {
                        CellularTCPIP_disconnect();
                        sState = ss_waitingForDisconnect;
                    }
                    break;
                case cs_disconnected :
                    if (isEnabled && SystemTime_timeHasArrived(&nextConnectAttemptTime)) {
                        CharString_define(40, server);
                        EEPROMStorage_getIPConsoleServerAddress(&server);
                        const uint16_t port = EEPROMStorage_ipConsoleServerPort();
                        CellularTCPIP_connect(&server, port, dataReceiver, statusCallback);
                        sState = ss_waitingForTCPIPConnecting;
                    }
                    break;
                default:
                    break;
            }
            }
            break;
        case ss_waitingForTCPIPConnecting :
            if (CellularTCPIP_connectionStatus() == cs_connecting) {
                sState = ss_waitingForTCPIPConnection;
            }
            break;
        case ss_waitingForTCPIPConnection : {
            const CellularTCPIPConnectionStatus connStatus = CellularTCPIP_connectionStatus();
            switch (connStatus) {
                case cs_connected :
                    // got a connection.
                    sState = ss_idle;
                    break;
                case cs_disconnected :
                    // failed to get a connection - try again a bit later
                    SystemTime_futureTime(200, &nextConnectAttemptTime);
                    sState = ss_idle;
                    break;
                default:
                    break;
            }
            }
            break;
        case ss_waitingForTCPIPSendingData :
            if (CellularTCPIP_connectionStatus() == cs_sendingData) {
                sState = ss_waitingForTCPIPSentData;
            }
            break;
        case ss_waitingForTCPIPSentData :
            if (CellularTCPIP_connectionStatus() != cs_sendingData) {
                // send complete
                sendDataProvider = 0;
                sState = ss_idle;
            }
            break;
        case ss_waitingForDisconnect :
            if (CellularTCPIP_connectionStatus() == cs_disconnected) {
                // disconnect complete
                sState = ss_idle;
            }
            break;
    }
}

void TCPIPConsole_enable (
    const bool saveToEEPROM)
{
    isEnabled = true;
    if (saveToEEPROM) {
        EEPROMStorage_setIPConsoleEnabled(isEnabled);
    }
}

void TCPIPConsole_disable (
    const bool saveToEEPROM)
{
    isEnabled = false;
    if (saveToEEPROM) {
        EEPROMStorage_setIPConsoleEnabled(isEnabled);
    }
}

void TCPIPConsole_restoreEnablement (void)
{
    isEnabled = EEPROMStorage_ipConsoleEnabled();
}

bool TCPIPConsole_isEnabled (void)
{
    return isEnabled;
}

void TCPIPConsole_setDataReceiver (
    SIM800_IPDataCallback receiver)
{
    dataReceiver = receiver;
}

bool TCPIPConsole_readyToSend (void)
{
    return ((CellularTCPIP_connectionStatus() == cs_connected) &&
            (sendDataProvider == 0));
}

void TCPIPConsole_sendData (
    CellularTCPIP_DataProvider dataProvider,
    CellularTCPIP_SendCompletionCallback completionCallback)
{
    sendDataProvider = dataProvider;
    sendCompletionCallback = completionCallback;
}

