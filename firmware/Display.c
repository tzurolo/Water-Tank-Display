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
#include <avr/io.h>
#include <avr/pgmspace.h>

#define HEADER_HEIGHT 30

#define TANK_WIDTH 300
#define TANK_HEIGHT 250
#define TANK_X 100
#define TANK_Y 50
#define TANK_WALL_THICKNESS 8
#define TANK_WALL_COLOR HX8357_BLACK

#define WATER_GAP_AT_TOP 20
#define WATER_HEIGHT (TANK_HEIGHT - (TANK_WALL_THICKNESS + WATER_GAP_AT_TOP))

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
static uint8_t waterLevel;  // percent full
static uint8_t lastDisplayedWaterLevel;
static uint16_t waterY; // relative to top of tank
static uint32_t lastDisplayedTimeSeconds;
static int16_t lastDisplayedTemperature;
static uint8_t lastDisplayBatteryPercent;
static uint8_t lastDisplaySignalQuality;
static bool lastDisplayMainsOn;
static bool lastDisplayPumpOn;

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

        currentText.x = TFT_HXD8357D_width - 120;
        currentText.y = 5;
        CharString_clear(&currentTextString);
        if (curTime.seconds > 43200L) { // max UTC offset
            curTime.seconds += (((int32_t)EEPROMStorage_utcOffset()) * 3600);
        }
        SystemTime_appendToString(&curTime, &currentTextString);
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_GREEN;
        currentText.fgColor = HX8357_BLACK;
        return &currentText;
    } else if (lastDisplayedWaterLevel != waterLevel) {
        // water level changed
        lastDisplayedWaterLevel = waterLevel;

        currentText.x = (TFT_HXD8357D_width / 2) - 20;
        currentText.y = TANK_Y + waterY + 10;
        CharString_clear(&currentTextString);
        StringUtils_appendDecimal(waterLevel, 1, 0, &currentTextString);
        CharString_appendC('%', &currentTextString);
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_BLUE;
        currentText.fgColor = HX8357_WHITE;
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
            CharString_appendC('%', &currentTextString);
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
        CharString_appendC(mainsOn ? 'M' : '/', &currentTextString);
        CharStringSpan_init(&currentTextString, &currentText.chars);
        currentText.bgColor = HX8357_GREEN;
        currentText.fgColor = HX8357_RED;
        return &currentText;
    } else if (pumpOn != lastDisplayPumpOn) {
        lastDisplayPumpOn = pumpOn;
        currentText.x = 195;
        currentText.y = 5;
        CharString_clear(&currentTextString);
        CharString_appendC(pumpOn ? 'P' : '/', &currentTextString);
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

    lastDisplayedWaterLevel = 0;
    lastDisplayedTimeSeconds = 0;
    lastDisplayedTemperature = 0;
    lastDisplayBatteryPercent = 0;
    lastDisplaySignalQuality = 0;
    lastDisplayMainsOn = false;
}

void Display_setWaterLevel (
    const uint8_t level)
{
    waterLevel = (level > 100) ? 100 : level;
    waterY = ((((uint16_t)(100 - waterLevel)) * WATER_HEIGHT) / 100) + WATER_GAP_AT_TOP;
    rState = (waterLevel < 100) ? rs_drawAir : rs_drawWater;
    lastDisplayedWaterLevel = 0;
}

void Display_task (void)
{
    switch (dState) {
        case ds_initial:
#if 1
            waterLevel = 10;
    waterY = ((((uint16_t)(100 - waterLevel)) * WATER_HEIGHT) / 100) + WATER_GAP_AT_TOP;
            rState = rs_drawHeader;
            rSubState = 0;
            TFT_HXD8357D_setRectangleSource(rectangleSource);
#endif
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
