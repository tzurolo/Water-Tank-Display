//
//  Cellular Communications using SIM800 Quad-band GSM/GPRS module
//
//
#ifndef CELLULARCOMM_H
#define CELLULARCOMM_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include "CharStringSpan.h"
#include "SIM800.h"

extern void CellularComm_Initialize (void);

// called by cell enable/disable commands
extern void CellularComm_Enable (void);
extern void CellularComm_Disable (void);

// called by mainloop task "scheduler"
extern void CellularComm_task (void);

extern bool CellularComm_isEnabled (void);
extern bool CellularComm_isIdle (void);

extern void CellularComm_requestRegistrationStatus (void);
extern bool CellularComm_gotRegistrationStatus (void);
extern uint8_t CellularComm_registrationStatus (void);
extern bool CellularComm_isRegistered (void);

extern const SIM800_NetworkTime* CellularComm_currentTime (void);
extern uint8_t CellularComm_SignalQuality (void);
extern int CellularComm_state (void);
extern bool CellularComm_stateIsTCPIPSubtask(const int state);
extern uint16_t CellularComm_batteryMillivolts (void);
extern uint8_t CellularComm_batteryPercent (void);

// posts the current contents of the outgoing message text
// to the given phone number
extern void CellularComm_setOutgoingSMSMessageNumber (
    const CharStringSpan_t* phoneNumber);

#endif  // CELLULARCOMM_H
