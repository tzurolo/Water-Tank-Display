//
//  TCPIP Console
//
//  Allows monitoring and control over TCP/IP
//
#ifndef TCPIPCONSOLE_H
#define TCPIPCONSOLE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "CellularTCPIP_SIM800.h"
#include "SIM800.h"

// comes up enabled by default
extern void TCPIPConsole_Initialize (void);

extern void TCPIPConsole_task (void);

extern void TCPIPConsole_enable (
    const bool saveToEEPROM);
extern void TCPIPConsole_disable (
    const bool saveToEEPROM);
extern void TCPIPConsole_restoreEnablement (void);
extern bool TCPIPConsole_isEnabled (void);

extern void TCPIPConsole_setDataReceiver (
    SIM800_IPDataCallback receiver);

extern bool TCPIPConsole_readyToSend (void);

extern void TCPIPConsole_sendData (
    CellularTCPIP_DataProvider dataProvider,
    CellularTCPIP_SendCompletionCallback completionCallback);

#endif  /* TCPIPCONSOLE_H */
