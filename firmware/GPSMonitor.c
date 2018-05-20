//
//  GPS monitor
//
//      Configures the GPS receiver and uses the TCPIP Console to 
//      communicate with the remote host
//
//
#include "GPSMonitor.h"

#include "SystemTime.h"
#include "SkyTraqVenus6.h"
#include "EEPROMStorage.h"
//#include "Thingspeak.h"
#include "CommandProcessor.h"
#include "Console.h"
#include "StringUtils.h"
#include "TCPIPConsole.h"
#include "CellularComm_SIM800.h"

typedef enum SendDataStatus_enum {
    sds_sending,
    sds_completedSuccessfully,
    sds_completedFailed
} SendDataStatus;
// Send data subtask states
typedef enum SendDataSubtaskState_enum {
    sdss_idle,
    sdss_sendingNavData
} SendDataSubtaskState;
// GPS configuration process states
typedef enum GPSConfigState_enum {
    gcs_initial,
    gcs_powerupDelay,
    gcs_waitingForMessageTypeConfigResponse,
    gcs_waitingForSysPosRateConfigResponse,
    gcs_waitingForNavModeConfigResponse,
    gcs_waitingForWAASConfigResponse,
    gcs_configurationComplete
} GPSConfigState;
// state variables
static GPSConfigState gcState = gcs_initial;
CharString_define(60, gpsNavDataHostMessage);
static bool haveGPSDataToSend;
static SendDataStatus sendDataStatus;
static SendDataSubtaskState sdsState;
static SkyTraqVenus6_CommandResponse gpsCommandResponse;
static SystemTime_t delayTime;

static void gpsCommandResponseCallback (
    const SkyTraqVenus6_CommandResponse msg)
{
    gpsCommandResponse = msg;
}

static void gpsNavigationDataCallback (
    const SkyTraqVenus6_NavDataMessage *navData)
{
    CharString_copyP(PSTR("["), &gpsNavDataHostMessage);

    // put in GPS info
    StringUtils_appendDecimal(navData->fixMode, 1, 0, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal(navData->numSVinFix, 1, 0, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal(navData->GPSWeek, 1, 0, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal32(navData->TimeOfWeek, 1, 2, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal32(navData->latitude, 1, 7, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal32(navData->longitude, 1, 7, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal32(navData->HDOP, 1, 2, &gpsNavDataHostMessage);

    // add in battery voltage and cellular signal quality
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal32(CellularComm_batteryMillivolts(), 1, 3, &gpsNavDataHostMessage);
    CharString_appendP(PSTR(","), &gpsNavDataHostMessage);
    StringUtils_appendDecimal32(CellularComm_SignalQuality(), 1, 0, &gpsNavDataHostMessage);

    CharString_appendP(PSTR("]"), &gpsNavDataHostMessage);

    Console_printCS(&gpsNavDataHostMessage);

    haveGPSDataToSend = true;
}

static void gpsConfigSubtask (void)
{
    switch (gcState) {
        case gcs_initial :
            SystemTime_futureTime(2, &delayTime);
            gcState = gcs_powerupDelay;
            break;
        case gcs_powerupDelay :
            if (SystemTime_timeHasArrived(&delayTime)) {
                Console_printP(PSTR("Configuring GPS..."));
                if (!SkyTraqVenus6_configMessageType(mt_BinaryMessage)) {
                    Console_printP(PSTR("error sending message"));
                }
                gpsCommandResponse = cr_noResponseYet;
                gcState = gcs_waitingForMessageTypeConfigResponse;
            }
            break;
        case gcs_waitingForMessageTypeConfigResponse :
            if (gpsCommandResponse == cr_ack) {
                SkyTraqVenus6_configSysPosRate(1);
                gpsCommandResponse = cr_noResponseYet;
                gcState = gcs_waitingForSysPosRateConfigResponse;
            } else if (gpsCommandResponse == cr_nack) {
                gcState = gcs_initial;
            }
            break;
        case gcs_waitingForSysPosRateConfigResponse :
            if (gpsCommandResponse == cr_ack) {
                SkyTraqVenus6_configNavMode(nm_car);
                gpsCommandResponse = cr_noResponseYet;
                gcState = gcs_waitingForNavModeConfigResponse;
            } else if (gpsCommandResponse == cr_nack) {
                gcState = gcs_initial;
            }
            break;
        case gcs_waitingForNavModeConfigResponse :
            if (gpsCommandResponse == cr_ack) {
                SkyTraqVenus6_configWAAS(true);
                gpsCommandResponse = cr_noResponseYet;
                gcState = gcs_waitingForWAASConfigResponse;
            } else if (gpsCommandResponse == cr_nack) {
                gcState = gcs_initial;
            }
            break;
        case gcs_waitingForWAASConfigResponse :
            if (gpsCommandResponse == cr_ack) {
                Console_printP(PSTR("GPS config successful"));
                gcState = gcs_configurationComplete;
            } else if (gpsCommandResponse == cr_nack) {
                gcState = gcs_initial;
            }
            break;
        case gcs_configurationComplete :
            break;
    }
}

static void navDataSender (void)
{
    CellularTCPIP_writeDataCS(&gpsNavDataHostMessage);
}

static void TCPIPSendCompletionCallaback (
    const bool success)
{
    sendDataStatus = success
        ? sds_completedSuccessfully
        : sds_completedFailed;   
}

static void SendDataSubtask (void)
{
    switch (sdsState) {
        case sdss_idle :
            if (haveGPSDataToSend && TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                TCPIPConsole_sendData(navDataSender, TCPIPSendCompletionCallaback);
                sdsState = sdss_sendingNavData;
            }
            break;
        case sdss_sendingNavData :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    haveGPSDataToSend = false;
                    sdsState = sdss_idle;
                    break;
                case sds_completedFailed :
                    sdsState = sdss_idle;
                    break;
            }
            break;
    }
}

void GPSMonitor_Initialize (void)
{
    SkyTraqVenus6_Initialize();

    haveGPSDataToSend = false;
    
    gcState = gcs_initial;
    sdsState = sdss_idle;
    gpsCommandResponse = cr_noResponseYet;

    SkyTraqVenus6_setCommandResponseCallback(gpsCommandResponseCallback);
    SkyTraqVenus6_setNavigationDataCallback(gpsNavigationDataCallback);
}

void GPSMonitor_task (void)
{
    gpsConfigSubtask();
    SkyTraqVenus6_task();
    SendDataSubtask();
}

