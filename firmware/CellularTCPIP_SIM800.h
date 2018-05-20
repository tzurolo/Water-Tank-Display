//
//  Cellular Communications, TCP/IP sesssions for SIM800
//
//  You can poll for connection status using CellularTCPIP_connectionStatus() or
//  you can use the callback you pass to the connect function.
//
#ifndef CELLULARTCPIP_H
#define CELLULARTCPIP_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/pgmspace.h>
#include "CharString.h"
#include "SIM800.h"

// TCP/IP connection status
typedef enum CellularTCPIPConnectionStatus_enum {
    cs_connecting,
    cs_connected,
    cs_sendingData,
    cs_disconnecting,
    cs_disconnected
} CellularTCPIPConnectionStatus;

// prototypes for functions that clients supply to
// provide and receive data
typedef bool (*CellularTCPIP_DataProvider)(void);   // return true when done
typedef void (*CellularTCPIP_SendCompletionCallback)(const bool success);
typedef void (*CellularTCPIP_ConnectionStateChangeCallback)(const CellularTCPIPConnectionStatus status);

extern void CellularTCPIP_Initialize (void);

extern CellularTCPIPConnectionStatus CellularTCPIP_connectionStatus (void);

extern void CellularTCPIP_connect (
    const CharString_t *hostAddress,
    const uint16_t hostPort,
    SIM800_IPDataCallback receiver,    // will be called when data from host arrives 
    CellularTCPIP_ConnectionStateChangeCallback stateChangeCallback);

extern void CellularTCPIP_sendData (
    CellularTCPIP_DataProvider provider,   // will be called to write data
    CellularTCPIP_SendCompletionCallback completionCallback);

extern void CellularTCPIP_disconnect (void);

extern void CellularTCPIP_notifyConnectionClosed (void);

extern bool CellularTCPIP_hasSubtaskWorkToDo (void);

extern void CellularTCPIP_Subtask (void);

extern int CellularTCPIP_state (void);

// these functions are to be called only by DataProvider
// functions
extern uint16_t CellularTCPIP_availableSpaceForWriteData (void);
extern void CellularTCPIP_writeDataP (
    PGM_P data);
extern void CellularTCPIP_writeDataCS (
    CharString_t *data);
extern void CellularTCPIP_writeDataCSS (
    CharStringSpan_t *data);

#endif  // CELLULARTCPIP_H
