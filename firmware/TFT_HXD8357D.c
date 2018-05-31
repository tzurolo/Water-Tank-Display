//
//  Adafruit 3.5" TFT 320x480 + Touchscreen Breakout Board w/MicroSD Socket - HXD8357D
//
#include "TFT_HXD8357D.h"

#include <avr/io.h>
#include "SystemTime.h"

#define RST_OUTPORT  PORTF
#define RST_PIN      PF0
#define RST_DIR      DDRF

#define LITE_OUTPORT  PORTB
#define LITE_PIN      PB6
#define LITE_DIR      DDRB

static uint8_t backlightPWMCount;
static uint8_t backlightBrightness; // 0 - 10, 0 is off, 10 is full on

// special characters
const uint16_t powerIcon[] = {
0b0000000000000000,
0b0001100000110000,
0b0001100000110000,
0b0001100000110000,
0b0001100000110000,
0b1111111111111110,
0b1111111111111110,
0b1111111111111110,
0b0111111111111100,
0b0000111111100000,
0b0000001110000000,
0b0000001110000000,
0b0000001110000000
};

// this function is used to PWM the display backlight. It is called
// 4800 times per second. We count off 10 ticks for each PWM cycle,
// which gives us 10 levels of backlight brightness. This gives us
// a PWM frequency of 480Hz
static void systemTimeTick (void)
{
    if (backlightBrightness >= 10) {
        LITE_OUTPORT |= (1 << LITE_PIN);
    } else if (backlightBrightness > 0) {
        if (backlightPWMCount < 9) {
            ++backlightPWMCount;
            if (backlightPWMCount == backlightBrightness) {
                LITE_OUTPORT &= ~(1 << LITE_PIN);
            }
        } else {
            // end of cycle - turn backlight on
            LITE_OUTPORT |= (1 << LITE_PIN);
            backlightPWMCount = 0;
        }
    } else {
        LITE_OUTPORT &= ~(1 << LITE_PIN);
    }
}

void TFT_HXD8357D_Initialize (void)
{
    // make LITE pin an output, initially high
    LITE_OUTPORT |= (1 << LITE_PIN);
    LITE_DIR  |= (1 << LITE_PIN);

    // make RST pin an output, initially high
    RST_OUTPORT |= (1 << RST_PIN);
    RST_DIR  |= (1 << RST_PIN);

    backlightPWMCount = 0;
    backlightBrightness = 10;
    SystemTime_registerForTickNotification(systemTimeTick);
}

void TFT_HXD8357D_setBacklightBrightness (
    const uint8_t brightness)
{
    backlightBrightness = brightness;
}

