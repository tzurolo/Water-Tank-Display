//
//  Display
//

#include "Display.h"

#include "TFT_HXD8357D.h"
#include "PowerMonitor.h"
#include "EEPROMStorage.h"
#include "CharString.h"
#include "StringUtils.h"
#include "SystemTime.h"
#include "InternalTemperatureMonitor.h"
#include "CellularComm_SIM800.h"
#include "PowerMonitor.h"
#include "DisplayFonts.h"
#include <avr/io.h>
#include <avr/pgmspace.h>

#define HEADER_HEIGHT 30

#define TANK_WIDTH 300
#define TANK_HEIGHT 240
#define TANK_X 90
#define TANK_Y 40
#define TANK_WALL_THICKNESS 8
#define TANK_WALL_COLOR HX8357_BLACK

#define WATER_GAP_AT_TOP 15
#define WATER_HEIGHT (TANK_HEIGHT - (TANK_WALL_THICKNESS + WATER_GAP_AT_TOP))

// water level below this will be displayed on top of water
#define WATER_TEXT_MIN_LEVEL 15

typedef enum DispalyState_enum {
    ds_initial,
    ds_idle
} DisplayState;

typedef enum RectangleState_enum {
    rs_drawHeader,
    rs_drawBody,
    rs_drawTankOutline,
    rs_drawAir,
    rs_drawWater,
    rs_done
} RectangleState;

static DisplayState dState;
static RectangleState rState;
static RectangleState prevRState;
static uint8_t rSubState;
static TFT_HXD8357D_Rectangle currentRectangle;
static TFT_HXD8357D_Text currentText;
CharString_define(40, currentTextString);
static int8_t waterLevel;  // percent full, or -1 if water level unkown
static int8_t lastDisplayedWaterLevel;
static uint32_t waterLevelTimestamp;
static uint32_t lastDisplayedWaterLevelTimestamp;
static uint16_t waterY; // relative to top of tank
static uint32_t lastDisplayedTimeSeconds;
static int16_t lastDisplayedTemperature;
static uint8_t lastDisplayBatteryPercent;
static uint8_t lastDisplaySignalQuality;
static bool lastDisplayMainsOn;
static bool lastDisplayPumpOn;

static char spacePadP[] PROGMEM = "   ";

// will be called by TFT_HXD8357D to get the next rectangle to draw, if any
static const TFT_HXD8357D_Rectangle* rectangleSource (void)
{
    prevRState = rState;
    switch (rState) {
        case rs_drawHeader :
            currentRectangle.x = 0;
            currentRectangle.y = 0;
            currentRectangle.width = TFT_HXD8357D_width;
            currentRectangle.height = HEADER_HEIGHT;
            currentRectangle.color = HX8357_GREEN;
            rState = rs_drawBody;
            break;
        case rs_drawBody :
            currentRectangle.x = 0;
            currentRectangle.y = HEADER_HEIGHT;
            currentRectangle.width = TFT_HXD8357D_width;
            currentRectangle.height = (TFT_HXD8357D_height - HEADER_HEIGHT);
            currentRectangle.color = HX8357_WHITE;
            rState = rs_drawTankOutline;
            break;
        case rs_drawTankOutline :
            switch (rSubState) {
                case 0 :
                    currentRectangle.x = TANK_X;
                    currentRectangle.y = TANK_Y;
                    currentRectangle.width = TANK_WALL_THICKNESS;
                    currentRectangle.height = TANK_HEIGHT;
                    currentRectangle.color = TANK_WALL_COLOR;
                    break;
                case 1 :
                    currentRectangle.x = TANK_X;
                    currentRectangle.y = TANK_Y + (TANK_HEIGHT - TANK_WALL_THICKNESS);
                    currentRectangle.width = TANK_WIDTH;
                    currentRectangle.height = TANK_WALL_THICKNESS;
                    currentRectangle.color = TANK_WALL_COLOR;
                    break;
                case 2 :
                    currentRectangle.x = TANK_X + (TANK_WIDTH - TANK_WALL_THICKNESS);
                    currentRectangle.y = TANK_Y;
                    currentRectangle.width = TANK_WALL_THICKNESS;
                    currentRectangle.height = TANK_HEIGHT;
                    currentRectangle.color = TANK_WALL_COLOR;
                    rState = rs_drawAir;
                    break;
            }
            break;
        case rs_drawAir:
            currentRectangle.x = TANK_X + TANK_WALL_THICKNESS;
            currentRectangle.y = TANK_Y + WATER_GAP_AT_TOP;
            currentRectangle.width = TANK_WIDTH - (2 * TANK_WALL_THICKNESS);
            currentRectangle.height = waterY - WATER_GAP_AT_TOP;
            currentRectangle.color = HX8357_WHITE;
            rState = (waterLevel > 0) ? rs_drawWater : rs_done;
            break;
        case rs_drawWater:
            currentRectangle.x = TANK_X + TANK_WALL_THICKNESS;
            currentRectangle.y = TANK_Y + waterY;
            currentRectangle.width = TANK_WIDTH - (2 * TANK_WALL_THICKNESS);
            currentRectangle.height = TANK_HEIGHT - (waterY + TANK_WALL_THICKNESS);
            currentRectangle.color = HX8357_BLUE;
            rState = rs_done;
            break;
        case rs_done :
            return NULL;
            break;
    }
    if (rState != prevRState) {
        // new state
        // reset substate
        rSubState = 0;
    } else {
        // same state
        // advance substate;
        ++rSubState;
    }
    return &currentRectangle;
}

static const TFT_HXD8357D_Text* textSource (void)
{
    const bool haveValidTemp = InternalTemperatureMonitor_haveValidSample();
    const int16_t temperature = haveValidTemp ? InternalTemperatureMonitor_currentTemperature() : 0;
    const uint8_t batteryPercent = CellularComm_batteryPercent();
    const uint8_t signalQuality = CellularComm_SignalQuality();
    const bool mainsOn = PowerMonitor_mainsOn();
    const bool pumpOn = PowerMonitor_pumpOn();
    SystemTime_t curTime;
    SystemTime_getCurrentTime(&curTime);
    if (curTime.seconds != lastDisplayedTimeSeconds) {
        // time changed - update display
        lastDisplayedTimeSeconds = curTime.seconds;

        currentText.x = TFT_HXD8357D_width - 160;
        currentText.y = 5;
        CharString_clear(&currentTextString);
        if (curTime.seconds > 43200L) { // max UTC offset
            curTime.seconds += (((int32_t)EEPROMStorage_utcOffset()) * 3600);
        }
        SystemTime_appendToString(&curTime, true, &currentTextString);
        CharString_appendP(PSTR("  "), &currentTextString);
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_GREEN;
        currentText.fgColor = HX8357_BLACK;
        return &currentText;
    } else if (waterLevel != lastDisplayedWaterLevel) {
        // water level changed
        lastDisplayedWaterLevel = waterLevel;

        currentText.x = (TANK_X + (TANK_WIDTH / 2)) - 20;
        CharString_clear(&currentTextString);
        if (waterLevel >= 0) {
            currentText.y = TANK_Y + waterY;
            if (waterLevel < WATER_TEXT_MIN_LEVEL) {
                currentText.y -= DisplayFonts_fontHeight(DisplayFonts_primary()) + 5;
            } else {
                currentText.y += 10;
            }
            StringUtils_appendDecimal(waterLevel, 1, 0, &currentTextString);
            CharString_appendC('%', &currentTextString);
        } else {
            currentText.y = TANK_Y + (WATER_HEIGHT / 2);
            CharString_appendC('?', &currentTextString);
        }
        CharStringSpan_init(&currentTextString, &currentText.chars);
        if (waterLevel < WATER_TEXT_MIN_LEVEL) {
            currentText.bgColor = HX8357_WHITE;
            currentText.fgColor = HX8357_BLUE;
        } else {
            currentText.bgColor = HX8357_BLUE;
            currentText.fgColor = HX8357_WHITE;
        }
        return &currentText;
    } else if (waterLevelTimestamp != lastDisplayedWaterLevelTimestamp) {
        // water level timestamp changed
        lastDisplayedWaterLevelTimestamp = waterLevelTimestamp;

        SystemTime_t ts;
        ts.seconds = waterLevelTimestamp;
        ts.seconds += (((int32_t)EEPROMStorage_utcOffset()) * 3600);
        ts.hundredths = 0;
        currentText.x = TANK_X + 30;
        currentText.y = TFT_HXD8357D_height - DisplayFonts_fontHeight(DisplayFonts_primary());
        CharString_copyP(PSTR("as of "), &currentTextString);
        SystemTime_appendToString(&ts, true, &currentTextString);
        CharString_appendP(PSTR("  "), &currentTextString);
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_WHITE;
        currentText.fgColor = HX8357_BLACK;
        return &currentText;
    } else if (temperature != lastDisplayedTemperature) {
        lastDisplayedTemperature = temperature;
        if (haveValidTemp) {
            currentText.x = 0;
            currentText.y = 5;
            CharString_copyP(PSTR("T:"), &currentTextString);
            StringUtils_appendDecimal(temperature, 1, 0, &currentTextString);
            CharStringSpan_init(&currentTextString, &currentText.chars);
            currentText.bgColor = HX8357_GREEN;
            currentText.fgColor = HX8357_BLACK;
            return &currentText;
        } else {
            // TODO: erase temp field
            return NULL;
        }
    } else if (batteryPercent != lastDisplayBatteryPercent) {
        lastDisplayBatteryPercent = batteryPercent;
        if (batteryPercent != 0) {
            currentText.x = 75;
            currentText.y = 5;
            CharString_copyP(PSTR("B:"), &currentTextString);
            StringUtils_appendDecimal(batteryPercent, 1, 0, &currentTextString);
            CharString_appendP(PSTR("%  "), &currentTextString);
            CharStringSpan_init(&currentTextString, &currentText.chars);
            currentText.bgColor = HX8357_GREEN;
            currentText.fgColor = HX8357_BLACK;
            return &currentText;
        } else {
            // TODO: erase field
            return NULL;
        }
    } else if (signalQuality != lastDisplaySignalQuality) {
        lastDisplaySignalQuality = signalQuality;
        if (signalQuality != 0) {
            currentText.x = 250;
            currentText.y = 5;
            CharString_copyP(PSTR("Q:"), &currentTextString);
            StringUtils_appendDecimal(signalQuality, 1, 0, &currentTextString);
            CharString_appendP(spacePadP, &currentTextString);
            CharStringSpan_init(&currentTextString, &currentText.chars);
            currentText.bgColor = HX8357_GREEN;
            currentText.fgColor = HX8357_BLACK;
            return &currentText;
        } else {
            // TODO: erase field
            return NULL;
        }
    } else if (mainsOn != lastDisplayMainsOn) {
        lastDisplayMainsOn = mainsOn;
        currentText.x = 170;
        currentText.y = 5;
        CharString_clear(&currentTextString);
        if (mainsOn) {
            CharString_appendC('M', &currentTextString);
        } else {
            CharString_appendP(spacePadP, &currentTextString);
        }
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_GREEN;
        currentText.fgColor = HX8357_RED;
        return &currentText;
    } else if (pumpOn != lastDisplayPumpOn) {
        lastDisplayPumpOn = pumpOn;
        currentText.x = 195;
        currentText.y = 5;
        CharString_clear(&currentTextString);
        if (pumpOn) {
            CharString_appendC('P', &currentTextString);
        } else {
            CharString_appendP(spacePadP, &currentTextString);
        }
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_GREEN;
        currentText.fgColor = HX8357_BLUE;
        return &currentText;
    } else {
        return NULL;
    }
}

void Display_Initialize (void)
{
    dState = ds_initial;

    lastDisplayedWaterLevel = -2;
    lastDisplayedWaterLevelTimestamp = 0;
    lastDisplayedTimeSeconds = 0;
    lastDisplayedTemperature = 0;
    lastDisplayBatteryPercent = 0;
    lastDisplaySignalQuality = 0;
    lastDisplayMainsOn = false;
}

void Display_setWaterLevel (
    const int8_t level,
    const uint32_t *levelTimestamp)
{
    waterLevel = (level > 100) ? 100 : level;
    waterY = (
        (waterLevel > 0)
        ? ((((uint16_t)(100 - waterLevel)) * WATER_HEIGHT) / 100)
        : WATER_HEIGHT) +
        WATER_GAP_AT_TOP;
    rState = (waterLevel < 100) ? rs_drawAir : rs_drawWater;
    lastDisplayedWaterLevel = -2;   // force update
    waterLevelTimestamp = *levelTimestamp;
}

void Display_task (void)
{
    switch (dState) {
        case ds_initial:
            rState = rs_drawHeader;
            rSubState = 0;
            waterLevel = -1;
            waterY = WATER_HEIGHT + WATER_GAP_AT_TOP;
            TFT_HXD8357D_setRectangleSource(rectangleSource);
            TFT_HXD8357D_setTextSource(textSource);
            dState = ds_idle;
            break;
        case ds_idle:
            break;
    }

    // manage display brightness
    TFT_HXD8357D_setBacklightBrightness(
        PowerMonitor_mainsOn()
        ? EEPROMStorage_LCDMainsOnBrightness()
        : EEPROMStorage_LCDMainsOffBrightness());
}
