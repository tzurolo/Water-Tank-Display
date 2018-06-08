//
// EEPROM Storage
//

#include "EEPROMStorage.h"

#include "EEPROM_Util.h"
#include "CharString.h"
#include "avr/pgmspace.h"

// This prevents the MSVC editor from tripping over EEMEM in definitions
#ifndef EEMEM
#define EEMEM
#endif

// storage address map
uint8_t EEMEM initFlag = 1; // initialization flag. Unprogrammed EE comes up as all one's

uint16_t EEMEM unitID = 0;
uint32_t EEMEM lastRebootTimeSec = 0;
uint16_t EEMEM rebootInterval = 1440;   // one day
char EEMEM cellPIN[8]  = "7353";
char tzPinP[]  PROGMEM = "7353"; // Tony's telestial Sim card PIN
int16_t EEMEM tempCalOffset = 327;
uint8_t EEMEM watchdogTimerCal = 100;
uint16_t EEMEM monitorTaskTimeout = 90;
uint8_t EEMEM notification = 0;
uint16_t EEMEM loggingUpdateInterval = 600;
uint16_t EEMEM loggingUpdateDelay = 70;
uint8_t EEMEM timeoutState = 0;

// internet
//char EEMEM apn[40] = "mobiledata";    // T-Mobile
char EEMEM apn[40]  = "hologram";        // hologram.io
char apnP[] PROGMEM = "hologram";
uint8_t EEMEM cipqsend = 0;

// thingspeak
uint8_t EEMEM thingspeakEnabled = 0;
char EEMEM thingspeakHostAddress[40]  = "api.thingspeak.com";
char thingspeakHostAddressP[] PROGMEM = "api.thingspeak.com";
uint16_t EEMEM thingspeakHostPort = 80;
char EEMEM thingspeakWriteKey[20] = "DD7TVSCZEHKZLAQP";
char thingspeakWriteKeyP[] PROGMEM = "DD7TVSCZEHKZLAQP";

// TCPIP Console
uint8_t EEMEM ipConsoleEnabled = 1;
char EEMEM ipConsoleServerAddress[32] = "tonyz.dyndns.org";
char tzHostAddressP[]         PROGMEM = "tonyz.dyndns.org";
uint16_t EEMEM ipConsoleServerPort = 3010;

static void getCharStringSpanFromP (
    PGM_P pStr,
    CharString_t *stringBuffer,
    CharStringSpan_t *span)
{
    CharString_copyP(pStr, stringBuffer);
    CharStringSpan_init(stringBuffer, span);
}

void EEPROMStorage_Initialize (void)
{
    // check if EE has been initialized
    const uint8_t iFlag = EEPROM_read(&initFlag);
    const uint8_t initLevel = (iFlag == 0xFF) ? 0 : iFlag;

    if (initLevel < 1) {
        // EE has not been initialized. Initialize now.
        CharString_define(40, stringBuffer);
        CharStringSpan_t stringBufferSpan;

        EEPROMStorage_setUnitID(3);

        EEPROMStorage_setLastRebootTimeSec(0);
        EEPROMStorage_setRebootInterval(1440);

        getCharStringSpanFromP(tzPinP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setPIN(&stringBufferSpan);
        EEPROMStorage_setCipqsend(0);

        EEPROMStorage_setTempCalOffset(325);
        EEPROMStorage_setWatchdogTimerCal(94);
        EEPROMStorage_setMonitorTaskTimeout(90);

        EEPROMStorage_setNotification(false);

        getCharStringSpanFromP(apnP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setAPN(&stringBufferSpan);

        EEPROMStorage_setLoggingUpdateInterval(600);
        EEPROMStorage_setLoggingUpdateDelay(70);
        EEPROMStorage_setThingspeak(false);
        getCharStringSpanFromP(thingspeakHostAddressP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setThingspeakHostAddress(&stringBufferSpan);
        EEPROMStorage_setThingspeakHostPort(80);
        getCharStringSpanFromP(thingspeakWriteKeyP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setThingspeakWriteKey(&stringBufferSpan);

        EEPROMStorage_setIPConsoleEnabled(false);
        getCharStringSpanFromP(tzHostAddressP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setIPConsoleServerAddress(&stringBufferSpan);
        EEPROMStorage_setIPConsoleServerPort(3010);

        // indicate EE has been initialized
        EEPROM_write(&initFlag, 1);
    }
}

void EEPROMStorage_setUnitID (
    const uint16_t id)
{
    EEPROM_writeWord((uint16_t*)&unitID, id);
}

uint16_t EEPROMStorage_unitID (void)
{
    return EEPROM_readWord((uint16_t*)&unitID);
}

void EEPROMStorage_setLastRebootTimeSec (
    const uint32_t sec)
{
    EEPROM_writeLong((uint32_t*)&lastRebootTimeSec, sec);
}

uint32_t EEPROMStorage_lastRebootTimeSec (void)
{
    return EEPROM_readLong((uint32_t*)&lastRebootTimeSec);
}

void EEPROMStorage_setRebootInterval (
    const uint16_t rebootMinutes)
{
    EEPROM_writeWord((uint16_t*)&rebootInterval, rebootMinutes);
}

uint16_t EEPROMStorage_rebootInterval (void)
{
    return EEPROM_readWord((uint16_t*)&rebootInterval);
}

void EEPROMStorage_setPIN (
    const CharStringSpan_t *PIN)
{
    EEPROM_writeString(cellPIN, sizeof(cellPIN), PIN);
}

void EEPROMStorage_getPIN (
    CharString_t *PIN)
{
    EEPROM_readString(cellPIN, PIN);
}

void EEPROMStorage_setTempCalOffset (
    const int16_t offset)
{
    EEPROM_writeWord((uint16_t*)&tempCalOffset, offset);
}

int16_t EEPROMStorage_tempCalOffset (void)
{
    return EEPROM_readWord((uint16_t*)&tempCalOffset);
}

void EEPROMStorage_setWatchdogTimerCal (
    const uint8_t wdtCal)
{
    EEPROM_write((uint8_t*)&watchdogTimerCal, wdtCal);
}

uint8_t EEPROMStorage_watchdogTimerCal (void)
{
    return EEPROM_read((uint8_t*)&watchdogTimerCal);
}

void EEPROMStorage_setMonitorTaskTimeout (
    const uint16_t wlmTimeout)
{
    EEPROM_writeWord((uint16_t*)&monitorTaskTimeout, wlmTimeout);
}
uint16_t EEPROMStorage_monitorTaskTimeout (void)
{
    return EEPROM_readWord((uint16_t*)&monitorTaskTimeout);
}

void EEPROMStorage_setNotification (
    const bool onOff)
{
    EEPROM_write(&notification, onOff ? 1 : 0);
}

bool EEPROMStorage_notificationEnabled (void)
{
    return (EEPROM_read(&notification) == 1);
}

void EEPROMStorage_setTimeoutState (
    const uint8_t state)
{
    EEPROM_write(&timeoutState, state);
}

uint8_t EEPROMStorage_timeoutState (void)
{
    return EEPROM_read(&timeoutState);
}

void EEPROMStorage_setAPN (
    const CharStringSpan_t *APN)
{
    EEPROM_writeString(apn, sizeof(apn), APN);
}

void EEPROMStorage_getAPN (
    CharString_t *APN)
{
    EEPROM_readString(apn, APN);
}

void EEPROMStorage_setCipqsend (
    const uint8_t qsend)
{
    EEPROM_write(&cipqsend, qsend);
}

uint8_t EEPROMStorage_cipqsend (void)
{
    return EEPROM_read(&cipqsend);
}

void EEPROMStorage_setLoggingUpdateInterval (
    const uint16_t updateInterval)
{
    EEPROM_writeWord(&loggingUpdateInterval, updateInterval);
}

uint16_t EEPROMStorage_LoggingUpdateInterval (void)
{
    return EEPROM_readWord(&loggingUpdateInterval);
}

void EEPROMStorage_setLoggingUpdateDelay (
    const uint16_t updateDelay)
{
    EEPROM_writeWord(&loggingUpdateDelay, updateDelay);
}

uint16_t EEPROMStorage_LoggingUpdateDelay (void)
{
    return EEPROM_readWord(&loggingUpdateDelay);
}

void EEPROMStorage_setThingspeak (
    const bool enabled)
{
    EEPROM_write(&thingspeakEnabled, enabled ? 1 : 0);
}

bool EEPROMStorage_thingspeakEnabled (void)
{
    return (EEPROM_read(&thingspeakEnabled) == 1);
}

void EEPROMStorage_setThingspeakHostAddress (
    const CharStringSpan_t *address)
{
    EEPROM_writeString(thingspeakHostAddress, sizeof(thingspeakHostAddress), address);
}

void EEPROMStorage_getThingspeakHostAddress (
    CharString_t *address)
{
    EEPROM_readString(thingspeakHostAddress, address);
}

void EEPROMStorage_setThingspeakHostPort (
    const uint16_t port)
{
    EEPROM_writeWord(&thingspeakHostPort, port);
}

uint16_t EEPROMStorage_thingspeakHostPort (void)
{
    return EEPROM_readWord(&thingspeakHostPort);
}

void EEPROMStorage_setThingspeakWriteKey (
    const CharStringSpan_t *writekey)
{
    EEPROM_writeString(thingspeakWriteKey, sizeof(thingspeakWriteKey), writekey);
}

void EEPROMStorage_getThingspeakWriteKey (
    CharString_t *writekey)
{
    EEPROM_readString(thingspeakWriteKey, writekey);
}

void EEPROMStorage_setIPConsoleEnabled (
    const bool enabled)
{
    EEPROM_write(&ipConsoleEnabled, enabled ? 1 : 0);
}

bool EEPROMStorage_ipConsoleEnabled (void)
{
    return EEPROM_read(&ipConsoleEnabled);
}

void EEPROMStorage_setIPConsoleServerAddress (
    const CharStringSpan_t *server)
{
    EEPROM_writeString(ipConsoleServerAddress, sizeof(ipConsoleServerAddress), server);
}

void EEPROMStorage_getIPConsoleServerAddress (
    CharString_t *server)
{
    EEPROM_readString(ipConsoleServerAddress, server);
}

void EEPROMStorage_setIPConsoleServerPort (
    const uint16_t port)
{
    EEPROM_writeWord(&ipConsoleServerPort, port);
}

uint16_t EEPROMStorage_ipConsoleServerPort (void)
{
    return EEPROM_readWord(&ipConsoleServerPort);
}
