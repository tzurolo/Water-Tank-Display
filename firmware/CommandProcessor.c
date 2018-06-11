//
// Command processor
//
// Interprets and executes commands from SMS or the console
//

#include "CommandProcessor.h"

#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/power.h>

#if BYTEQUEUE_HIGHWATERMARK_ENABLED
#include "SoftwareSerialRx0.h"
#include "SoftwareSerialTx.h"
#endif
#include "SystemTime.h"
#include "CellularComm_SIM800.h"
#include "CellularTCPIP_SIM800.h"
#include "Console.h"
#include "EEPROM_Util.h"
#include "EEPROMStorage.h"
#include "WaterLevelDisplay.h"
#include "StringUtils.h"
#include "UART_async.h"
#include "TFT_HXD8357D.h"
#include "PowerMonitor.h"
#include "SDCard.h"

typedef void (*StringProvider)(
    CharString_t *string);

CharString_define(60, CommandProcessor_incomingCommand)
CharString_define(100, CommandProcessor_commandReply)

// command keywords
static char pinP[]              PROGMEM = "PIN";
static char cipqsendP[]         PROGMEM = "cipqsend";
static char apnP[]              PROGMEM = "APN";
static char offP[]              PROGMEM = "off";
static char onP[]               PROGMEM = "on";
static char idP[]               PROGMEM = "id";
static char ipserverP[]         PROGMEM = "ipserver";
static char tCalOffsetP[]       PROGMEM = "tCalOffset";
static char wdtCalP[]           PROGMEM = "wdtCal";
static char batCalP[]           PROGMEM = "batCal";
static char wlmTimeoutP[]       PROGMEM = "wlmTimeout";
static char rebootP[]           PROGMEM = "reboot";
static char sampleIntervalP[]   PROGMEM = "sampleInterval";
static char logIntervalP[]      PROGMEM = "logInterval";
static char thingspeakP[]       PROGMEM = "thingspeak";

void CommandProcessor_createStatusMessage (
    CharString_t *msg)
{
    CharString_clear(msg);
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    SystemTime_appendToString(&curTime, msg);
    CharString_appendP(PSTR(",st:"), msg);
    StringUtils_appendDecimal(CellularComm_state(), 2, 0, msg);
    if (CellularComm_stateIsTCPIPSubtask(CellularComm_state())) {
        CharString_appendC('.', msg);
        StringUtils_appendDecimal(CellularTCPIP_state(), 2, 0, msg);
    }
    CharString_appendC(',', msg);
    StringUtils_appendDecimal(WaterLevelDisplay_state(), 1, 0, msg);
    CharString_appendP(PSTR(",Vc:"), msg);
    StringUtils_appendDecimal(CellularComm_batteryMillivolts(), 1, 3, msg);
    CharString_appendP(PSTR(",r:"), msg);
    StringUtils_appendDecimal((int)CellularComm_registrationStatus(), 1, 0, msg);
    CharString_appendP(PSTR(",q:"), msg);
    StringUtils_appendDecimal(CellularComm_SignalQuality(), 2, 0, msg);
    CharString_appendP(PSTR(",m:"), msg);
    StringUtils_appendDecimal(PowerMonitor_mainsOn(), 1, 0, msg);
    CharString_appendP(PSTR(",p:"), msg);
    StringUtils_appendDecimal(PowerMonitor_pumpOn(), 1, 0, msg);
    CharString_appendP(PSTR(",T:"), msg);
    uint8_t minTicks;
    uint8_t maxTicks;
    SystemTime_getTaskTickRange(&minTicks, &maxTicks);
    StringUtils_appendDecimal(minTicks, 1, 0, msg);
    CharString_appendC('-', msg);
    StringUtils_appendDecimal(maxTicks, 1, 0, msg);
    SystemTime_resetTaskTickRange();
    CharString_appendP(PSTR("  "), msg);
}

static int16_t scanIntegerToken (
    CharStringSpan_t *str,
    bool *isValid)
{
    StringUtils_skipWhitespace(str);
    int16_t value = 0;
    StringUtils_scanInteger(str, isValid, &value, str);
    return value;
}

static uint32_t scanIntegerU32Token (
    CharStringSpan_t *str,
    bool *isValid)
{
    StringUtils_skipWhitespace(str);
    uint32_t value = 0;
    StringUtils_scanIntegerU32(str, isValid, &value, str);
    return value;
}

static void beginJSON (
    CharString_t *str)
{
    CharString_copyP(PSTR("{"), str);
}

static void continueJSON (
    CharString_t *str)
{
    CharString_appendC(',', str);
}

static void endJSON (
    CharString_t *str)
{
    CharString_appendC('}', str);
}

static void appendJSONStrValue (
    PGM_P name,
    StringProvider strProvider,
    CharString_t *str)
{
    CharString_appendC('\"', str);
    CharString_appendP(name, str);
    CharString_appendP(PSTR("\":\""), str);
    strProvider(str);
    CharString_appendC('\"', str);
}

static void appendJSONIntValue (
    PGM_P name,
    const int16_t value,
    CharString_t *str)
{
    CharString_appendC('\"', str);
    CharString_appendP(name, str);
    CharString_appendP(PSTR("\":"), str);
    StringUtils_appendDecimal(value, 1, 0, str);
}

static void appendJSONTimeValue (
    PGM_P name,
    const SystemTime_t *time,
    CharString_t *str)
{
    CharString_appendC('\"', str);
    CharString_appendP(name, str);
    CharString_appendP(PSTR("\":\""), str);
    SystemTime_appendToString(time, str);
    CharString_appendC('\"', str);
}

static void makeJSONStrValue (
    PGM_P name,
    StringProvider strProvider,
    CharString_t *str)
{
    beginJSON(str);
    appendJSONStrValue(name, strProvider, str);
    endJSON(str);
}

static void makeJSONIntValue (
    PGM_P name,
    const int16_t value,
    CharString_t *str)
{
    beginJSON(str);
    appendJSONIntValue(name, value, str);
    endJSON(str);
}

bool CommandProcessor_executeCommand (
    const CharStringSpan_t* command,
    CharString_t *reply)
{
    bool validCommand = true;

    CharStringSpan_t cmd = *command;
    CharStringSpan_t cmdToken;
    StringUtils_scanToken(&cmd, &cmdToken);
    if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("status"))) {
        CharStringSpan_t statusToNumber;
        CharStringSpan_clear(&statusToNumber);
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("to"))) {
            StringUtils_scanToken(&cmd, &statusToNumber);
        }
	// return status info
	CommandProcessor_createStatusMessage(reply);
        if (!CharStringSpan_isEmpty(&statusToNumber)) {
            //CellularComm_setOutgoingSMSMessageNumber(&statusToNumber);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("t"))) {
        CharString_define(40, txt);
        CharString_copyP(PSTR("water 52%"), &txt);
        TFT_HXD8357D_setText(&txt);
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("lcd"))) {
        const uint8_t mainsOnBrightness = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            EEPROMStorage_setLCDMainsOnBrightness(mainsOnBrightness);
        }
        const uint8_t mainsOffBrightness = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            EEPROMStorage_setLCDMainsOffBrightness(mainsOffBrightness);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("data"))) {
        const uint8_t waterLevel = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            WaterLevelDisplay_setDataFromHost(waterLevel);
            const uint32_t serverTime = scanIntegerU32Token(&cmd, &validCommand);
            if (validCommand && (serverTime != 0)) {
                SystemTime_setTimeAdjustment(&serverTime);
            }
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("tset"))) {
        const uint32_t serverTime = scanIntegerU32Token(&cmd, &validCommand);
        if (validCommand && (serverTime != 0)) {
            SystemTime_setTimeAdjustment(&serverTime);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("set"))) {
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, idP)) {
            const uint8_t id = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setUnitID(id);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, tCalOffsetP)) {
            const int16_t tempCalOffset = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setTempCalOffset(tempCalOffset);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, wdtCalP)) {
            const uint8_t wdtCal = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setWatchdogTimerCal(wdtCal);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, wlmTimeoutP)) {
            const uint16_t wlmTimeout = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setMonitorTaskTimeout(wlmTimeout);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, rebootP)) {
            const uint16_t rebootInterval = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setRebootInterval(rebootInterval);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, apnP)) {
            StringUtils_scanToken(&cmd, &cmdToken);
            if (!CharStringSpan_isEmpty(&cmdToken)) {
                EEPROMStorage_setAPN(&cmdToken);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, cipqsendP)) {
            const uint16_t qsend = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setCipqsend(qsend);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, ipserverP)) {
            StringUtils_scanToken(&cmd, &cmdToken);
            const uint16_t ipPort = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setIPConsoleServerAddress(&cmdToken);
                EEPROMStorage_setIPConsoleServerPort(ipPort);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, logIntervalP)) {
            const uint16_t loggingInterval = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROMStorage_setLoggingUpdateInterval(loggingInterval);
            }
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, thingspeakP)) {
            StringUtils_scanToken(&cmd, &cmdToken);
            if (CharStringSpan_equalsNocaseP(&cmdToken, onP)) {
                EEPROMStorage_setThingspeak(true);
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, offP)) {
                EEPROMStorage_setThingspeak(false);
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("address"))) {
                StringUtils_scanToken(&cmd, &cmdToken);
                if (!CharStringSpan_isEmpty(&cmdToken)) {
                    EEPROMStorage_setThingspeakHostAddress(&cmdToken);
                }
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("port"))) {
                const uint16_t port = scanIntegerToken(&cmd, &validCommand);
                if (validCommand) {
                    EEPROMStorage_setThingspeakHostPort(port);
                }
            } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("writekey"))) {
                StringUtils_scanToken(&cmd, &cmdToken);
                if (!CharStringSpan_isEmpty(&cmdToken)) {
                    EEPROMStorage_setThingspeakWriteKey(&cmdToken);
                }
            } else {
                validCommand = false;
            }
        } else {
            validCommand = false;
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("get"))) {
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, pinP)) {
            makeJSONStrValue(pinP, EEPROMStorage_getPIN, reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, idP)) {
            makeJSONIntValue(idP, EEPROMStorage_unitID(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, tCalOffsetP)) {
            makeJSONIntValue(tCalOffsetP, EEPROMStorage_tempCalOffset(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, wdtCalP)) {
            makeJSONIntValue(wdtCalP, EEPROMStorage_watchdogTimerCal(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, wlmTimeoutP)) {
            makeJSONIntValue(wlmTimeoutP, EEPROMStorage_monitorTaskTimeout(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, rebootP)) {
            makeJSONIntValue(rebootP, EEPROMStorage_rebootInterval(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, apnP)) {
            makeJSONStrValue(apnP, EEPROMStorage_getAPN, reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, cipqsendP)) {
            makeJSONIntValue(cipqsendP, EEPROMStorage_cipqsend(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, ipserverP)) {
            beginJSON(reply);
            appendJSONStrValue(PSTR("IP_Addr"), EEPROMStorage_getIPConsoleServerAddress, reply);
            continueJSON(reply);
            appendJSONIntValue(PSTR("IP_Port"), EEPROMStorage_ipConsoleServerPort(), reply);
            endJSON(reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, logIntervalP)) {
            makeJSONIntValue(logIntervalP, EEPROMStorage_LoggingUpdateInterval(), reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, thingspeakP)) {
            beginJSON(reply);
            appendJSONIntValue(PSTR("TS_En"), EEPROMStorage_thingspeakEnabled() ? 1 : 0, reply);
            continueJSON(reply);
            appendJSONStrValue(PSTR("TS_Addr"), EEPROMStorage_getThingspeakHostAddress, reply);
            continueJSON(reply);
            appendJSONIntValue(PSTR("TS_Port"), EEPROMStorage_thingspeakHostPort(), reply);
            continueJSON(reply);
            appendJSONStrValue(PSTR("TS_WK"), EEPROMStorage_getThingspeakWriteKey, reply);
            endJSON(reply);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("time"))) {
            beginJSON(reply);
            SystemTime_t time;
            SystemTime_getCurrentTime(&time);
            appendJSONTimeValue(PSTR("CurTime"), &time, reply);
            continueJSON(reply);
            time.seconds = SystemTime_uptime();
            time.hundredths = 0;
            appendJSONTimeValue(PSTR("uptime"), &time, reply);
            endJSON(reply);
        } else {
            validCommand = false;
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("notify"))) {
        StringUtils_scanToken(&cmd, &cmdToken);
        if (CharStringSpan_equalsNocaseP(&cmdToken, onP)) {
            EEPROMStorage_setNotification(true);
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, offP)) {
            EEPROMStorage_setNotification(false);
        } else {
            validCommand = false;
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("sms"))) {
        // get number to send to
        CharStringSpan_t recipientNumber;
        StringUtils_scanToken(&cmd, &recipientNumber);
        if ((CharStringSpan_length(&recipientNumber) > 5) &&
            (CharStringSpan_front(&recipientNumber) == '+')) {
            // got a phone number
            // get the message to send. it should be a quoted string
            CharStringSpan_t message;
            StringUtils_scanQuotedString(&cmd, &message, NULL);
            if (!CharStringSpan_isEmpty(&message)) {
                // got the message
                CharString_copyIters(
                    CharStringSpan_begin(&message),
                    CharStringSpan_end(&message),
                    reply);
                //CellularComm_setOutgoingSMSMessageNumber(&recipientNumber);
            }
        } else {
            validCommand = false;
        }
#if BYTEQUEUE_HIGHWATERMARK_ENABLED
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("bqhw"))) {
        // byte queue report highwater
        SoftwareSerialRx0_reportHighwater();
        SoftwareSerialRx2_reportHighwater();
        SoftwareSerialTx_reportHighwater();
        UART_reportHighwater();
#endif
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, rebootP)) {
        // reboot
        SystemTime_commenceShutdown();
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("eeread"))) {
        const uint16_t eeAddr = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            beginJSON(reply);
            appendJSONIntValue(PSTR("EEAddr"), eeAddr, reply);
            continueJSON(reply);
            appendJSONIntValue(PSTR("EEVal"), EEPROM_read((uint8_t*)eeAddr), reply);
            endJSON(reply);
        }
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("eewrite"))) {
        const uint16_t eeAddr = scanIntegerToken(&cmd, &validCommand);
        if (validCommand) {
            const uint16_t eeValue = scanIntegerToken(&cmd, &validCommand);
            if (validCommand) {
                EEPROM_write((uint8_t*)eeAddr, eeValue);
            }
        }
    } else {
        validCommand = false;
        Console_printP(PSTR("unrecognized command"));
    }

    return validCommand;
}
//                uint32_t i1 = 30463UL;
//                uint32_t i2 = 30582UL;
#if 0
// sleep code
        } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("sleep"))) {
            //clock_prescale_set(clock_div_256);
            uint16_t sleepTime = 8;
            cmdToken = strtok(NULL, tokenDelimiters);
            if (cmdToken != NULL) {
                sleepTime = atoi(cmdToken);
            }
            // disable ADC (move to ADCManager)
            ADCSRA &= ~(1 << ADEN);
            power_all_disable();
            // disable all digital inputs
            DIDR0 = 0x3F;
            DIDR1 = 3;
            // turn off all pullups
            PORTB = 0;
            PORTC = 0;
            PORTD = 0;
            SystemTime_sleepFor(sleepTime);
            power_all_enable();
            ADCManager_Initialize();
            BatteryMonitor_Initialize();
            InternalTemperatureMonitor_Initialize();
            UltrasonicSensorMonitor_Initialize();
            SoftwareSerialRx0_Initialize();
            SoftwareSerialRx2_Initialize();
            Console_Initialize();
            CellularComm_Initialize();
            CellularTCPIP_Initialize();
            TCPIPConsole_Initialize();
#endif