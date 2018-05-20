//
//  LED Lighting UPS monitor
//
//      uses the hardware UART to communicate with LED lighting UPS
//      use the TCPIP Console to communicate with the remote host
//
//
#include "LEDLightingUPSMonitor.h"

#include "SystemTime.h"
#include "EEPROMStorage.h"
//#include "Thingspeak.h"
#include "CommandProcessor.h"
#include "Console.h"
#include "UART_async.h"
#include "StringUtils.h"
#include "TCPIPConsole.h"

// time in seconds
#define ReadInterval 10
#define UPS_SERIAL_DATA_TIMEOUT 4
#define UPS_SERIAL_DATA_LENGTH 32

typedef enum SendDataStatus_enum {
    sds_sending,
    sds_completedSuccessfully,
    sds_completedFailed
} SendDataStatus;
// Send data subtask states
typedef enum SendDataSubtaskState_enum {
    sdss_idle,
    sdss_sendingSettings,
    sdss_sendingStatus
} SendDataSubtaskState;
// UPS monitor states
typedef enum UPSMonitorState_enum {
    ums_waitingForSerialData,
    ums_readingSettings
} UPSMonitorState;
// UPS command states
typedef enum UPSCommandState_enum {
    ucs_idle,
    ucs_waitingForUPSSettings,
    ucs_waitingForUPSStatusMessage
} UPSCommandState;
// state variables
static UPSMonitorState umState = ums_waitingForSerialData;
static UPSCommandState ucState = ucs_idle;
static const char tokenDelimiters[] = " \n\r";
static SystemTime_t readTimeoutTime;
static SystemTime_t nextScheduledReadingTime;
static SystemTime_t thingspeakDelayTime;
static bool gotUPSSerialData;
CharString_define(UPS_SERIAL_DATA_LENGTH, upsSerialData);
static bool gotUPSStatus;
CharString_define(UPS_SERIAL_DATA_LENGTH, latestUpsStatusMessage);
static bool gotUPSSettings;
CharString_define(60, latestUpsSettings);
CharString_define(20, clientCommand);
static bool haveUPSStatusToSend;
static bool haveUPSSettingsToSend;
static SendDataStatus sendDataStatus;
static SendDataSubtaskState sdsState;

static UPSStatus upsStatus;
static int16_t latestUPSVoltage;
static int16_t latestACAdapterVoltage;
static int16_t latestBatteryVoltage;
static int16_t latestTemperature;


static void parseDecimal (
    const char* str,
    const uint8_t expectedFractionalDigits,
    bool *dataValid,
    int16_t *value)
{
    bool isValid;
    uint8_t numFractionalDigits;
    StringUtils_scanDecimal(str, &isValid, value, &numFractionalDigits);
    if (isValid) {
        if (numFractionalDigits != expectedFractionalDigits) {
            *dataValid = false;
        }
    } else {
        *dataValid = false;
    }
}

static void parseUPSSerialData (
    CharString_t *serialData)
{
    char serialDataBuf[UPS_SERIAL_DATA_LENGTH + 1];
    strncpy(serialDataBuf, CharString_cstr(serialData),
        UPS_SERIAL_DATA_LENGTH - 1);
    serialDataBuf[UPS_SERIAL_DATA_LENGTH] = 0;
    const char* dataToken = strtok(serialDataBuf, tokenDelimiters);
    bool dataValid = true;
	while (dataToken != NULL) {
        switch (dataToken[0]) {
            case 'U' : // UPS status
                switch (dataToken[1]) {
                    case '0' :
                        upsStatus = upss_poweringDown;
                        break;
                    case '1' :
                        upsStatus = upss_onBattery;
                        break;
                    case '2' :
                        upsStatus = upss_onACAdapter;
                        break;
                    default :
                        upsStatus = upss_unknown;
                        dataValid = false;
                        break;
                }
                break;
            case 'B' :  // Battery voltage
                parseDecimal(&dataToken[1], 2, &dataValid, &latestBatteryVoltage);
                break;
            case 'A' : // ACAdapter voltage
                parseDecimal(&dataToken[1], 2, &dataValid, &latestACAdapterVoltage);
                break;
            case 'T' : // temperature
                parseDecimal(&dataToken[1], 1, &dataValid, &latestTemperature);
                break;
            default :
                dataValid = false;
                break;
        }

        dataToken = strtok(NULL, tokenDelimiters);
    }
    if (!dataValid) {
        Console_printP(PSTR("error parsing UPS serial data"));
    }
}

static void readUPSSerialData (void)
{
    char byte;
    while (UART_read_byte(&byte)) {
        if (byte == '\r') {
            if (!CharString_isEmpty(&upsSerialData)) {
                gotUPSSerialData = true;
            }
        } else if (byte == '\n') {
        } else {
            CharString_appendC(byte, &upsSerialData);
        }
    }
}

static void UPSMonitorSubtask (void)
{
    switch (umState) {
        case ums_waitingForSerialData :
            if (gotUPSSerialData) {
                // let's see what kind of message we got
                const char firstChar = CharString_at(&upsSerialData, 0);
                switch (firstChar) {
                    case 'U' :
                        //parseUPSSerialData(&upsSerialData);
                        CharString_copyCS(&upsSerialData, &latestUpsStatusMessage);
                        gotUPSStatus = true;
                        break;
                    case '{' :
                        CharString_clear(&latestUpsSettings);
                        umState = ums_readingSettings;
                        break;
                    default:
                        // unrecognized - ignore
                        break;
                }
                gotUPSSerialData = false;
                CharString_clear(&upsSerialData);
           }
           break;
        case ums_readingSettings :
            // check for timeout
            if (gotUPSSerialData) {
                const char firstChar = CharString_at(&upsSerialData, 0);
                if (firstChar == '}') {
                    // end of settings
                    gotUPSSettings = true;
                    umState = ums_waitingForSerialData;
                } else {
                    // append setting
                    if (!CharString_isEmpty(&latestUpsSettings)) {
                        // punctuate with newline
                        CharString_appendC('\n', &latestUpsSettings);
                    }
                    CharString_appendCS(&upsSerialData, &latestUpsSettings);
                    gotUPSSerialData = false;
                    CharString_clear(&upsSerialData);
                }
            }
            break;
    }
}

static void UPSCommandSubtask (void)
{
    switch (ucState) {
        case ucs_idle :
            if (upsStatus == upss_poweringDown) {
                //umState = ums_waitingToSendNotification;
            } else if (!CharString_isEmpty(&clientCommand)) {
                // if the command is to change a setting, we will need to
                // re-read the settings
                if (CharString_startsWithP(&clientCommand, PSTR("set"))) {
                    gotUPSSettings = false;
                }
                Console_printP(PSTR("sending client command"));
                UART_write_string(CharString_cstr(&clientCommand));
                UART_write_byte('\r');
                CharString_clear(&clientCommand);
            } else if (!gotUPSSettings) {
                // need settings
                Console_printP(PSTR("requesting UPS settings"));
                UART_write_stringP(PSTR("settings"));
                UART_write_byte('\r');
                ucState = ucs_waitingForUPSSettings;
                SystemTime_futureTime(UPS_SERIAL_DATA_TIMEOUT, &readTimeoutTime);
            } else if (!gotUPSStatus) {
                Console_printP(PSTR("requesting UPS status"));
                UART_write_stringP(PSTR("status"));
                UART_write_byte('\r');
                ucState = ucs_waitingForUPSStatusMessage;
                SystemTime_futureTime(UPS_SERIAL_DATA_TIMEOUT, &readTimeoutTime);
            } else if (SystemTime_timeHasArrived(&nextScheduledReadingTime)) {
                // trigger request for status from UPS
                gotUPSStatus = false;
            }
            break;
        case ucs_waitingForUPSSettings :
            if (gotUPSSettings) {
                Console_printP(PSTR("got UPS settings"));
                Console_printCS(&latestUpsSettings);
                // send data to TCPIP console
                haveUPSSettingsToSend = true;

                ucState = ucs_idle;
            } else if (SystemTime_timeHasArrived(&readTimeoutTime)) {
                // read timed out - try again
                ucState = ucs_idle;
            }
            break;
        case ucs_waitingForUPSStatusMessage :
            if (gotUPSStatus) {
                // got status
                Console_printP(PSTR("got UPS status"));
                Console_printCS(&latestUpsStatusMessage);
                // send data to TCPIP console
                haveUPSStatusToSend = true;

                // schedule next reading
                SystemTime_futureTime(ReadInterval, &nextScheduledReadingTime);
                ucState = ucs_idle;
            } else if (SystemTime_timeHasArrived(&readTimeoutTime)) {
                // read timed out - try again
                ucState = ucs_idle;
            }
            break;
    }
}

static void settingsSender (void)
{
    CellularTCPIP_writeDataCS(&latestUpsSettings);
}

static void statusSender (void)
{
    CellularTCPIP_writeDataCS(&latestUpsStatusMessage);
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
            if (haveUPSSettingsToSend && TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                TCPIPConsole_sendData(settingsSender, TCPIPSendCompletionCallaback);
                sdsState = sdss_sendingSettings;
            } else if (haveUPSStatusToSend && TCPIPConsole_readyToSend()) {
                sendDataStatus = sds_sending;
                TCPIPConsole_sendData(statusSender, TCPIPSendCompletionCallaback);
                sdsState = sdss_sendingStatus;
            }
            break;
        case sdss_sendingSettings :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    haveUPSSettingsToSend = false;
                    sdsState = sdss_idle;
                    break;
                case sds_completedFailed :
                    sdsState = sdss_idle;
                    break;
            }
            break;
        case sdss_sendingStatus :
            switch (sendDataStatus) {
                case sds_sending :
                    break;
                case sds_completedSuccessfully :
                    haveUPSStatusToSend = false;
                    sdsState = sdss_idle;
                    break;
                case sds_completedFailed :
                    sdsState = sdss_idle;
                    break;
            }
            break;
    }
}

void LEDLightingUPSMonitor_Initialize (void)
{
    // give the system some time to initialize before monitoring
    SystemTime_futureTime(15, &nextScheduledReadingTime);

    CharString_clear(&upsSerialData);
    CharString_clear(&latestUpsSettings);
    CharString_clear(&latestUpsStatusMessage);
    CharString_clear(&clientCommand);
    gotUPSStatus = false;
    gotUPSSettings = false;
    haveUPSStatusToSend = false;
    haveUPSSettingsToSend = false;

    latestUPSVoltage = 0;
    
    gotUPSSerialData = false;
    umState = ums_waitingForSerialData;
    ucState = ucs_idle;
    sdsState = sdss_idle;

    upsStatus = upss_unknown;
    latestUPSVoltage = 0;
    latestBatteryVoltage = 0;
    latestACAdapterVoltage = 0;

    UART_set_baud_rate(300);
}

void LEDLightingUPSMonitor_task (void)
{
    readUPSSerialData();
    UPSMonitorSubtask();
    UPSCommandSubtask();
    SendDataSubtask();
}

void LEDLightingUPSMonitor_sendCommand (
    const char *command)
{
    CharString_copy(command, &clientCommand);
}

const CharString_t* LEDLightingUPSMonitor_UPSStatusStr (void)
{
    return &latestUpsStatusMessage;
}

const CharString_t* LEDLightingUPSMonitor_UPSSettings (void)
{
    return &latestUpsSettings;
}

int16_t LEDLightingUPSMonitor_UPSVoltage (void)
{
    return latestUPSVoltage;
}

int16_t LEDLightingUPSMonitor_BatteryVoltage (void)
{
    return latestBatteryVoltage;
}

int16_t LEDLightingUPSMonitor_ACAdapterVoltage (void)
{
    return latestACAdapterVoltage;
}

int16_t LEDLightingUPSMonitor_Temperature (void)
{
    return latestTemperature;
}

UPSStatus LEDLightingUPSMonitor_UPSStatus (void)
{
    return upsStatus;
}
