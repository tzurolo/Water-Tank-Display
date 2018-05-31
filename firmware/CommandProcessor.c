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

// beginning of tft display prototyping
#include <avr/io.h>
#include "util/delay.h"
#include "SPIAsync.h"

#define DC_OUTPORT  PORTB
#define DC_PIN      PB7
#define DC_DIR      DDRB

#define SS_SETUP_TIME 10

#define HX8357_NOP     0x00
#define HX8357_SWRESET 0x01
#define HX8357_RDDID   0x04
#define HX8357_RDDST   0x09

#define HX8357_RDPOWMODE  0x0A
#define HX8357_RDMADCTL  0x0B
#define HX8357_RDCOLMOD  0x0C
#define HX8357_RDDIM  0x0D
#define HX8357_RDDSDR  0x0F

#define HX8357_SLPIN   0x10
#define HX8357_SLPOUT  0x11
#define HX8357B_PTLON   0x12
#define HX8357B_NORON   0x13

#define HX8357_INVOFF  0x20
#define HX8357_INVON   0x21
#define HX8357_DISPOFF 0x28
#define HX8357_DISPON  0x29

#define HX8357_CASET   0x2A
#define HX8357_PASET   0x2B
#define HX8357_RAMWR   0x2C
#define HX8357_RAMRD   0x2E

#define HX8357B_PTLAR   0x30
#define HX8357_TEON  0x35
#define HX8357_TEARLINE  0x44
#define HX8357_MADCTL  0x36
#define HX8357_COLMOD  0x3A

#define HX8357_SETOSC 0xB0
#define HX8357_SETPWR1 0xB1
#define HX8357B_SETDISPLAY 0xB2
#define HX8357_SETRGB 0xB3
#define HX8357D_SETCOM  0xB6

#define HX8357B_SETDISPMODE  0xB4
#define HX8357D_SETCYC  0xB4
#define HX8357B_SETOTP 0xB7
#define HX8357D_SETC 0xB9

#define HX8357B_SET_PANEL_DRIVING 0xC0
#define HX8357D_SETSTBA 0xC0
#define HX8357B_SETDGC  0xC1
#define HX8357B_SETID  0xC3
#define HX8357B_SETDDB  0xC4
#define HX8357B_SETDISPLAYFRAME 0xC5
#define HX8357B_GAMMASET 0xC8
#define HX8357B_SETCABC  0xC9
#define HX8357_SETPANEL  0xCC


#define HX8357B_SETPOWER 0xD0
#define HX8357B_SETVCOM 0xD1
#define HX8357B_SETPWRNORMAL 0xD2

#define HX8357B_RDID1   0xDA
#define HX8357B_RDID2   0xDB
#define HX8357B_RDID3   0xDC
#define HX8357B_RDID4   0xDD

#define HX8357D_SETGAMMA 0xE0

#define HX8357B_SETGAMMA 0xC8
#define HX8357B_SETPANELRELATED  0xE9



// Color definitions
#define	HX8357_BLACK   0x0000
#define	HX8357_BLUE    0x001F
#define	HX8357_RED     0xF800
#define	HX8357_GREEN   0x07E0
#define HX8357_CYAN    0x07FF
#define HX8357_MAGENTA 0xF81F
#define HX8357_YELLOW  0xFFE0  
#define HX8357_WHITE   0xFFFF

static uint8_t maxWaitCycles = 0;

void spiWrite (const uint8_t byte)
{
    SPIAsync_sendByte(byte);
    uint8_t waitCycles = 0;
    while (!SPIAsync_operationCompleted()) ++waitCycles;
    if (waitCycles > maxWaitCycles) maxWaitCycles = waitCycles;
}

void spiWrite16 (const uint16_t word)
{
    spiWrite(word >> 8);
    spiWrite(word & 0xFF);
}

uint8_t spiRead (void)
{
    SPIAsync_requestByte();
    while (!SPIAsync_operationCompleted()); // wait for byte
    return SPIAsync_getByte();
}

void writeCommand (const uint8_t cmd)
{
    DC_OUTPORT &= ~(1 << DC_PIN);
    spiWrite(cmd);
    DC_OUTPORT |= (1 << DC_PIN);
}

void delay (const uint16_t ms)
{
    _delay_ms(ms);
}

void fillRect (
    const uint16_t x,
    const uint16_t y,
    const uint16_t w,
    const uint16_t h,
    const uint16_t c)
{
    SPIAsync_assertSS();
    _delay_us(SS_SETUP_TIME);

    // set addr window
    writeCommand(HX8357_CASET); // Column addr set
    spiWrite16(x);          // start column
    spiWrite16(x + w - 1);  // end column

    writeCommand(HX8357_PASET); // Row addr set
    spiWrite16(y);          // start row
    spiWrite16(y + h - 1);  // end row

    writeCommand(HX8357_RAMWR); // write to RAM

    // write pixels
    uint16_t color;
    switch (c) {
        case 1 : color = HX8357_BLUE;   break;
        case 2 : color = HX8357_RED;   break;
        case 3 : color = HX8357_GREEN;   break;
        case 4 : color = HX8357_CYAN;   break;
        case 5 : color = HX8357_MAGENTA;   break;
        case 6 : color = HX8357_YELLOW;   break;
        case 7 : color = HX8357_WHITE;   break;
        default : color = HX8357_BLACK; break;
    }
    uint8_t hi = color >> 8, lo = color;
    uint32_t len = ((uint32_t)w) * h;
    for (uint32_t t=len; t; --t){
        spiWrite(hi);
        spiWrite(lo);
    }
    SPIAsync_deassertSS();
}

// end of tft display prototyping

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
    CharString_appendP(PSTR(",sd:"), msg);
    StringUtils_appendDecimal(SDCard_isPresent(), 1, 0, msg);
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

    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("xi"))) {
        SPIAsync_init(
            SPIAsync_MODE_MASTER |
            SPIAsync_SPEED_FOSC_DIV_4 |
            SPIAsync_SCK_LEAD_RISING |
            SPIAsync_SCK_LEAD_SAMPLE |
            SPIAsync_ORDER_MSB_FIRST
            );
        DC_OUTPORT |= (1 << DC_PIN);
        DC_DIR  |= (1 << DC_PIN);

        SPIAsync_assertSS();
        _delay_us(SS_SETUP_TIME);
        writeCommand(HX8357_SWRESET);

    delay(10);
    // setextc
    writeCommand(HX8357D_SETC);
    spiWrite(0xFF);
    spiWrite(0x83);
    spiWrite(0x57);
    delay(300);

    // setRGB which also enables SDO
    writeCommand(HX8357_SETRGB); 
    spiWrite(0x80);  //enable SDO pin!
//    spiWrite(0x00);  //disable SDO pin!
    spiWrite(0x0);
    spiWrite(0x06);
    spiWrite(0x06);

    writeCommand(HX8357D_SETCOM);
    spiWrite(0x25);  // -1.52V
    
    writeCommand(HX8357_SETOSC);
    spiWrite(0x68);  // Normal mode 70Hz, Idle mode 55 Hz
    
    writeCommand(HX8357_SETPANEL); //Set Panel
    spiWrite(0x05);  // BGR, Gate direction swapped
    
    writeCommand(HX8357_SETPWR1);
    spiWrite(0x00);  // Not deep standby
    spiWrite(0x15);  //BT
    spiWrite(0x1C);  //VSPR
    spiWrite(0x1C);  //VSNR
    spiWrite(0x83);  //AP
    spiWrite(0xAA);  //FS

    writeCommand(HX8357D_SETSTBA);  
    spiWrite(0x50);  //OPON normal
    spiWrite(0x50);  //OPON idle
    spiWrite(0x01);  //STBA
    spiWrite(0x3C);  //STBA
    spiWrite(0x1E);  //STBA
    spiWrite(0x08);  //GEN
    
    writeCommand(HX8357D_SETCYC);  
    spiWrite(0x02);  //NW 0x02
    spiWrite(0x40);  //RTN
    spiWrite(0x00);  //DIV
    spiWrite(0x2A);  //DUM
    spiWrite(0x2A);  //DUM
    spiWrite(0x0D);  //GDON
    spiWrite(0x78);  //GDOFF

    writeCommand(HX8357D_SETGAMMA); 
    spiWrite(0x02);
    spiWrite(0x0A);
    spiWrite(0x11);
    spiWrite(0x1d);
    spiWrite(0x23);
    spiWrite(0x35);
    spiWrite(0x41);
    spiWrite(0x4b);
    spiWrite(0x4b);
    spiWrite(0x42);
    spiWrite(0x3A);
    spiWrite(0x27);
    spiWrite(0x1B);
    spiWrite(0x08);
    spiWrite(0x09);
    spiWrite(0x03);
    spiWrite(0x02);
    spiWrite(0x0A);
    spiWrite(0x11);
    spiWrite(0x1d);
    spiWrite(0x23);
    spiWrite(0x35);
    spiWrite(0x41);
    spiWrite(0x4b);
    spiWrite(0x4b);
    spiWrite(0x42);
    spiWrite(0x3A);
    spiWrite(0x27);
    spiWrite(0x1B);
    spiWrite(0x08);
    spiWrite(0x09);
    spiWrite(0x03);
    spiWrite(0x00);
    spiWrite(0x01);
    
    writeCommand(HX8357_COLMOD);
    spiWrite(0x55);  // 16 bit
    
    writeCommand(HX8357_MADCTL);  
    spiWrite(0xC0); 
    
    writeCommand(HX8357_TEON);  // TE off
    spiWrite(0x00); 
    
    writeCommand(HX8357_TEARLINE);  // tear line
    spiWrite(0x00); 
    spiWrite(0x02);

    writeCommand(HX8357_SLPOUT); //Exit Sleep
    delay(150);
    
    writeCommand(HX8357_DISPON);  // display on
    delay(50);

        SPIAsync_deassertSS();
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("xq"))) {
        // get sw self test result
        DC_OUTPORT &= ~(1 << DC_PIN);
        SPIAsync_assertSS();
        _delay_us(SS_SETUP_TIME);
        spiWrite(HX8357_RDDSDR);
        DC_OUTPORT |= (1 << DC_PIN);
        const uint8_t r = spiRead();
        SPIAsync_deassertSS();
        CharString_define(40, msg);
        CharString_copyP(PSTR("self-test: "), &msg);
        StringUtils_appendDecimal(r, 1, 0, &msg);
        Console_printCS(&msg);
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("xf"))) {
        // fill rectangle
        const uint16_t x = scanIntegerToken(&cmd, &validCommand);
        const uint16_t y = scanIntegerToken(&cmd, &validCommand);
        const uint16_t w = scanIntegerToken(&cmd, &validCommand);
        const uint16_t h = scanIntegerToken(&cmd, &validCommand);
        const uint16_t c = scanIntegerToken(&cmd, &validCommand);

        fillRect(x, y, w, h, c);

        CharString_define(40, msg);
        CharString_copyP(PSTR("fill: "), &msg);
        StringUtils_appendDecimal(x, 1, 0, &msg);
        CharString_appendC(',', &msg);
        StringUtils_appendDecimal(y, 1, 0, &msg);
        CharString_appendC(',', &msg);
        StringUtils_appendDecimal(w, 1, 0, &msg);
        CharString_appendC(',', &msg);
        StringUtils_appendDecimal(h, 1, 0, &msg);
        Console_printCS(&msg);
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("xn"))) {
        // draw line
        SPIAsync_assertSS();
        _delay_us(SS_SETUP_TIME);
        writeCommand(HX8357_INVON);
        SPIAsync_deassertSS();
    } else if (CharStringSpan_equalsNocaseP(&cmdToken, PSTR("b"))) {
        const uint8_t brightness = scanIntegerToken(&cmd, &validCommand);
        TFT_HXD8357D_setBacklightBrightness(brightness);


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