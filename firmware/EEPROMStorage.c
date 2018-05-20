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
uint8_t EEMEM batteryVoltageCal = 100;
uint16_t EEMEM monitorTaskTimeout = 60;
uint8_t EEMEM notification = 0;
uint16_t EEMEM sampleInterval = 600;
uint16_t EEMEM loggingUpdateInterval = 600;
uint8_t EEMEM timeoutState = 0;

// filter parameters
uint8_t EEMEM filterSampleTime = 1;     // once per second
uint8_t EEMEM numFilterSamples = 180;   // 3 minutes of samples
uint16_t EEMEM filterVariance = 16383;  // effectively unlimited variance

// tank parameters
uint16_t EEMEM emptyDist = 290;
uint16_t EEMEM fullDist = 30;
uint8_t EEMEM lowNotification = 50;
uint8_t EEMEM highNotification = 90;

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
        EEPROMStorage_setBatteryVoltageCal(98);
        EEPROMStorage_setMonitorTaskTimeout(60);

	EEPROMStorage_setWaterTankEmptyDistance(290);
	EEPROMStorage_setWaterTankFullDistance(30);

	EEPROMStorage_setWaterHighNotificationLevel(90);
	EEPROMStorage_setWaterLowNotificationLevel(45);

	EEPROMStorage_setNotification(false);

        getCharStringSpanFromP(apnP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setAPN(&stringBufferSpan);

        EEPROMStorage_setSampleInterval(600);
        EEPROMStorage_setLoggingUpdateInterval(600);
        EEPROMStorage_setThingspeak(false);
        getCharStringSpanFromP(thingspeakHostAddressP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setThingspeakHostAddress(&stringBufferSpan);
        EEPROMStorage_setThingspeakHostPort(80);
        getCharStringSpanFromP(thingspeakWriteKeyP, &stringBuffer, &stringBufferSpan);
        EEPROMStorage_setThingspeakWriteKey(&stringBufferSpan);

        EEPROMStorage_setFilterSampleTime(1);   // once per second
        EEPROMStorage_setFilterSamples(180);    // 3 minutes of data at once per second
        EEPROMStorage_setFilterVariance(16383); // effectively unlimited variance

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

void EEPROMStorage_setBatteryVoltageCal (
    const uint8_t batCal)
{
    EEPROM_write((uint8_t*)&batteryVoltageCal, batCal);
}

uint8_t EEPROMStorage_batteryVoltageCal (void)
{
    return EEPROM_read((uint8_t*)&batteryVoltageCal);
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

void EEPROMStorage_setWaterTankEmptyDistance (
    const uint16_t value)
{
    EEPROM_writeWord(&emptyDist, value);
}

uint16_t EEPROMStorage_waterTankEmptyDistance (void)
{
    return EEPROM_readWord(&emptyDist);
}

void EEPROMStorage_setWaterTankFullDistance (
    const uint16_t value)
{
    EEPROM_writeWord(&fullDist, value);
}

uint16_t EEPROMStorage_waterTankFullDistance (void)
{
    return EEPROM_readWord(&fullDist);
}

void EEPROMStorage_setWaterLowNotificationLevel (
    const uint8_t level)
{
    EEPROM_write(&lowNotification, level);
}

uint8_t EEPROMStorage_waterLowNotificationLevel (void)
{
    return EEPROM_read(&lowNotification);
}

void EEPROMStorage_setWaterHighNotificationLevel (
    const uint8_t level)
{
    EEPROM_write(&highNotification, level);
}

uint8_t EEPROMStorage_waterHighNotificationLevel (void)
{
    return EEPROM_read(&highNotification);
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

void EEPROMStorage_setSampleInterval (
    const uint16_t interval)
{
    EEPROM_writeWord(&sampleInterval, interval);
}

uint16_t EEPROMStorage_sampleInterval (void)
{
    return EEPROM_readWord(&sampleInterval);
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

void EEPROMStorage_setFilterSampleTime (
    const uint8_t sampleTime)
{
    EEPROM_write(&filterSampleTime, sampleTime);
}

uint8_t EEPROMStorage_filterSampleTime (void)
{
    return EEPROM_read(&filterSampleTime);
}

void EEPROMStorage_setFilterSamples (
    const uint8_t samples)
{
    EEPROM_write(&numFilterSamples, samples);
}

uint8_t EEPROMStorage_filterSamples (void)
{
    return EEPROM_read(&numFilterSamples);
}

void EEPROMStorage_setFilterVariance (
    const uint16_t variance)
{
    EEPROM_writeWord(&filterVariance, variance);
}

uint16_t EEPROMStorage_filterVariance (void)
{
    return EEPROM_readWord(&filterVariance);
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
