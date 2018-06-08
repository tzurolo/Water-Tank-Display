//
//  Water Level Display
//
//  Main logic of display, which also monitors the mains AC and the pump
//      Uses the TCPIP Console to communicate with the remote host
//
//  Pin usage:
//      PXX - mains power monitor - connected to USB power input
//
#include "WaterLevelDisplay.h"

#include "SystemTime.h"
#include "EEPROMStorage.h"
//#include "Thingspeak.h"
#include "CommandProcessor.h"
#include "Console.h"
#include "StringUtils.h"
#include "CellularComm_SIM800.h"
#include "TCPIPConsole.h"
#include "CommandProcessor.h"
#include "Display.h"

#define SW_VERSION 10

// water level state
typedef enum WaterLevel_enum {
    wl_inRange = 'I',
    wl_low = 'L',
    wl_high = 'H'
} WaterLevelState;

// Send data subtask states
typedef enum SendDataStatus_enum {
    sds_sending,
    sds_completedSuccessfully,
    sds_completedFailed
} SendDataStatus;

typedef enum CommandProcessingMode_enum {
    cpm_singleCommand,
    cpm_commandBlock
} CommandProcessingMode;


// state variables
static const char tokenDelimiters[] = " \n\r";
static WaterLevelDisplayState wldState;
static CommandProcessingMode commandMode = cpm_singleCommand;
static SendDataStatus sendDataStatus;
static SystemTime_t nextConnectTime;
static SystemTime_t time;   // used to measure how long it took to get a connection,
                            // and for the powerdown delay future time
static bool gotCommandFromHost;
static SystemTime_t connectStartTime;
static CharStringSpan_t remainingReplyDataToSend;
static uint8_t latestWaterLevelPercent;

#define DATA_SENDER_BUFFER_LEN 30

static bool sampleDataSender (void)
{
    bool sendComplete = false;

    // check to see if there is enough room in the output queue for our data. If
    // not, we check again next time we're called.
    if (CellularTCPIP_availableSpaceForWriteData() >= DATA_SENDER_BUFFER_LEN) {
        // there is room in the output queue for our data
        CharString_define(DATA_SENDER_BUFFER_LEN, dataToSend);
        // send per-post data
        CharString_copyP(PSTR("I"), &dataToSend);
        StringUtils_appendDecimal(EEPROMStorage_unitID(), 1, 0, &dataToSend);
        CharString_appendC('V', &dataToSend);
        StringUtils_appendDecimal(SW_VERSION, 1, 0, &dataToSend);
        CharString_appendC('B', &dataToSend);
        StringUtils_appendDecimal(CellularComm_batteryMillivolts(), 1, 0, &dataToSend);
        CharString_appendC('R', &dataToSend);
        StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, &dataToSend);
        CharString_appendC('Q', &dataToSend);
        StringUtils_appendDecimal(CellularComm_SignalQuality(), 1, 0, &dataToSend);
        SystemTime_t curTime;
        SystemTime_getCurrentTime(&curTime);
        const int32_t secondsSinceLastSample = SystemTime_diffSec(&curTime, &connectStartTime);
        CharString_appendP(PSTR("C"), &dataToSend);
        StringUtils_appendDecimal(secondsSinceLastSample, 1, 0, &dataToSend);
        CharString_appendC(';', &dataToSend);

        // append the delta time between the last sample and now, and append the terminator (Z)
        CharString_appendP(PSTR("Z\n"), &dataToSend);
        sendComplete = true;
        CellularTCPIP_writeDataCS(&dataToSend);
    }

    return sendComplete;
}

static bool replyDataSender (void)
{
    // send as much as there is enough room in the output queue for a chunk of our data.
    const uint16_t availableSpace = CellularTCPIP_availableSpaceForWriteData();
    const uint8_t spaceInSpan =
        (availableSpace > 255)
        ? 255
        : ((uint8_t)availableSpace);
    CharStringSpan_t chunk;
    CharStringSpan_extractLeft(spaceInSpan, &remainingReplyDataToSend, &chunk);
    CellularTCPIP_writeDataCSS(&chunk);

    return CharStringSpan_isEmpty(&remainingReplyDataToSend);
}

static void TCPIPSendCompletionCallaback (
    const bool success)
{
    sendDataStatus = success
        ? sds_completedSuccessfully
        : sds_completedFailed;   
}

static void IPDataCallback (
    const CharString_t *ipData)
{
    // RAMSentinel_printStackPtr();
    for (int i = 0; i < CharString_length(ipData); ++i) {
        const char c = CharString_at(ipData, i);
        if ((c == '\r') || (c == '\n')) {
            // got command terminator
            if (!CharString_isEmpty(&CommandProcessor_incomingCommand)) {
                gotCommandFromHost = true;
                if (CharString_length(&CommandProcessor_incomingCommand) == 1) {
                    // check for mode command characters
                    switch (CharString_at(&CommandProcessor_incomingCommand, 0)) {
                        case '[' :
                            commandMode = cpm_commandBlock;
                            CharString_clear(&CommandProcessor_incomingCommand);
                            break;
                        case ']' :
                            commandMode = cpm_singleCommand;
                            CharString_clear(&CommandProcessor_incomingCommand);
                            break;
                        default:
                            break;
                    }
                }
            }
        } else {
            gotCommandFromHost = false;
            CharString_appendC(c, &CommandProcessor_incomingCommand);
        }
    }
}

static void enableTCPIP (void)
{
    CellularComm_Enable();
    gotCommandFromHost = false;
    CharString_clear(&CommandProcessor_incomingCommand);
    commandMode = cpm_singleCommand;
    TCPIPConsole_setDataReceiver(IPDataCallback);
    TCPIPConsole_enable(false);
}

void initiatePowerdown (void)
{
    // give it a little while to properly close the connection
    SystemTime_futureTime(150, &time);
    Console_printP(PSTR("powering down"));
    wldState = wlds_delayBeforeDisable;
}

void transitionPerCommandMode(void)
{
    if (commandMode == cpm_commandBlock) {
        // more commands coming. wait for next command
        wldState = wlds_waitingForHostCommand;
        Console_printP(PSTR("wait for next command"));
    } else {
        // no more commands coming. initiate powerdown sequence
        initiatePowerdown();
    }
}

void WaterLevelDisplay_Initialize (void)
{
    wldState = wlds_initial;
    commandMode = cpm_singleCommand;
    gotCommandFromHost = false;
}

void WaterLevelDisplay_task (void)
{
    if ((wldState > wlds_waitingForNextConnectTime) &&
        (wldState < wlds_delayBeforeDisable) &&
        SystemTime_timeHasArrived(&time)) {
        // task has exceeded timeout
        Console_printP(PSTR("WLD timeout"));
        initiatePowerdown();
    }

    switch (wldState) {
        case wlds_initial :
//            wldState = (SystemTime_LastReboot() == lrb_software)
//                ? wlds_done         // starting up after software reboot
//                : wlds_resuming;    // hardware reboot
            wldState = wlds_waitingForNextConnectTime;
            break;
        case wlds_waitingForNextConnectTime :
            // determine if it's time to contact server
            if (SystemTime_timeHasArrived(&nextConnectTime)) {
                SystemTime_getCurrentTime(&connectStartTime);
                enableTCPIP();
                Console_printP(PSTR("time to connect"));
                wldState = wlds_waitingForSensorData;

                // set up overal task timeout
                SystemTime_futureTime(EEPROMStorage_monitorTaskTimeout() * 100, &time);
            }
            break;
        case wlds_waitingForSensorData :
            if (TCPIPConsole_isEnabled()) {
                wldState = wlds_waitingForConnection;
            } else {
                // just a sample interval
                SystemTime_futureTime(50, &time);
                wldState = wlds_done;
            }
            break;
        case wlds_waitingForConnection :
            if (TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                TCPIPConsole_sendData(sampleDataSender, TCPIPSendCompletionCallaback);
                wldState = wlds_sendingSampleData;
            }
            break;
        case wlds_sendingSampleData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    Console_printP(PSTR("success, wait for 1st cmd"));
                    wldState = wlds_waitingForHostCommand;
                    break;
                case sds_completedFailed :
                    Console_printP(PSTR("failed, wait for 1st cmd"));
                    wldState = wlds_waitingForHostCommand;
                    break;
            }
            break;
        case wlds_waitingForHostCommand:
            if (gotCommandFromHost) {
                gotCommandFromHost = false;
                CharString_clear(&CommandProcessor_commandReply);
                if (!CharString_isEmpty(&CommandProcessor_incomingCommand)) {
                    CharStringSpan_t cmd;
                    CharStringSpan_init(&CommandProcessor_incomingCommand, &cmd);
                    const bool successful =
                        CommandProcessor_executeCommand(&cmd, &CommandProcessor_commandReply);
                    CharString_clear(&CommandProcessor_incomingCommand);
                    if (CharString_isEmpty(&CommandProcessor_commandReply)) {
                        if (commandMode == cpm_commandBlock) {
                            // this will prompt the host for the next command
                            CharString_copyP(
                                successful
                                ? PSTR("OK\n")
                                : PSTR("ERROR\n"),
                                &CommandProcessor_commandReply);
                        }
                    } else {
                        CharString_appendC('\n', &CommandProcessor_commandReply);
                    }
                }
                if (SystemTime_shuttingDown()) {
                    initiatePowerdown();
                } else if (CharString_isEmpty(&CommandProcessor_commandReply)) {
                    transitionPerCommandMode();
                } else {
                    // prepare to send reply
                    wldState = wlds_waitingForReadyToSendReply;
                }
            }
            break;
        case wlds_waitingForReadyToSendReply :
            if (TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                CharStringSpan_init(&CommandProcessor_commandReply, &remainingReplyDataToSend);
                TCPIPConsole_sendData(replyDataSender, TCPIPSendCompletionCallaback);
                wldState = wlds_sendingReplyData;
            }
            break;
        case wlds_sendingReplyData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                case sds_completedFailed :
                    transitionPerCommandMode();
                    break;
            }
            break;
        case wlds_delayBeforeDisable :
            if (SystemTime_timeHasArrived(&time)) {
                TCPIPConsole_disable(false);
                CellularComm_Disable();
                // set a timeout for disable
                SystemTime_futureTime(1000, &time);
                wldState = wlds_waitingForCellularCommDisable;
            }
            break;
        case wlds_waitingForCellularCommDisable :
            if ((!CellularComm_isEnabled()) ||
                SystemTime_timeHasArrived(&time)) {
                wldState = wlds_done;
                Console_printP(PSTR("done"));
            }
            break;
        case wlds_done : {
            SystemTime_t curTime;
            SystemTime_getCurrentTime(&curTime);
            if (curTime.seconds < 200) {
                // we don't have server time yet - set it now
                // otherwise we will set it upon daily reboot
                SystemTime_applyTimeAdjustment();
            }
            // determine when to contact host
            const uint16_t loggingInterval = EEPROMStorage_LoggingUpdateInterval();
            SystemTime_getCurrentTime(&curTime);
            nextConnectTime.seconds =
                (((curTime.seconds + (loggingInterval / 2)) / loggingInterval) + 1) * loggingInterval;
            nextConnectTime.seconds += EEPROMStorage_LoggingUpdateDelay();
            nextConnectTime.hundredths = 0;
            wldState = wlds_waitingForNextConnectTime;
            }
            break;
    }
}

bool WaterLevelDisplay_taskIsDone (void)
{
    return wldState == wlds_done;
}

WaterLevelDisplayState WaterLevelDisplay_state (void)
{
    return wldState;
}

void WaterLevelDisplay_setDataFromHost (
    const uint8_t waterLevelPct)
{
    latestWaterLevelPercent = waterLevelPct;
    Display_setWaterLevel(waterLevelPct);

    CharString_define(40, msg);
    CharString_copyP(PSTR("Water Level: "), &msg);
    StringUtils_appendDecimal(waterLevelPct, 1, 0, &msg);
    CharString_appendC('%', &msg);
    Console_printCS(&msg);
}
