//
//  Water Level Display
//
//  Main logic of monitor
//
#ifndef WATERLEVELDISPLAY_H
#define WATERLEVELDISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "CharString.h"

typedef enum WaterLevelDisplayState_enum {
    wlds_initial,
    wlds_resuming,
    wlds_waitingForSensorData,
    wlds_waitingForConnection,
    wlds_sendingSampleData,
    wlds_waitingForHostCommand,
    wlds_waitingForReadyToSendReply,
    wlds_sendingReplyData,
    wlds_delayBeforeDisable,
    wlds_waitingForCellularCommDisable,
    wlds_poweringDown,
    wlds_done
} WaterLevelDisplayState;

// sets up control pins. called once at power-up
extern void WaterLevelDisplay_Initialize (void);

// called in each iteration of the mainloop
extern void WaterLevelDisplay_task (void);

extern bool WaterLevelDisplay_taskIsDone (void);

extern WaterLevelDisplayState WaterLeveDisplay_state (void);

#endif  // WATERLEVELDISPLAY_H
