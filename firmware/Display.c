//
//  Display
//

#include "Display.h"

#include "TFT_HXD8357D.h"

#define HEADER_HEIGHT 30
#define FOOTER_HEIGHT 30

#define TANK_WIDTH 220
#define TANK_HEIGHT 300
#define TANK_X 50
#define TANK_Y 100
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
    rs_drawFooter,
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
static uint8_t waterLevel;  // percent full
static uint16_t waterY; // relative to top of tank

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
            currentRectangle.height = (TFT_HXD8357D_height - (HEADER_HEIGHT + FOOTER_HEIGHT));
            currentRectangle.color = HX8357_WHITE;
            rState = rs_drawFooter;
            break;
        case rs_drawFooter :
            currentRectangle.x = 0;
            currentRectangle.y = TFT_HXD8357D_height - FOOTER_HEIGHT;
            currentRectangle.width = TFT_HXD8357D_width;
            currentRectangle.height = FOOTER_HEIGHT;
            currentRectangle.color = HX8357_YELLOW;
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
            currentRectangle.height = waterY;
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

void Display_Initialize (void)
{
    dState = ds_initial;
}

void Display_setWaterLevel (
    const uint8_t level)
{
    waterLevel = (level > 100) ? 100 : level;
    waterY = ((((uint16_t)(100 - waterLevel)) * WATER_HEIGHT) / 100) + WATER_GAP_AT_TOP;
    rState = (waterLevel < 100) ? rs_drawAir : rs_drawWater;
}

void Display_task (void)
{
    switch (dState) {
        case ds_initial:
            Display_setWaterLevel(10);
            rState = rs_drawHeader;
            rSubState = 0;
            TFT_HXD8357D_setRectangleSource(rectangleSource);
            dState = ds_idle;
            break;
        case ds_idle:
            break;
    }
}
