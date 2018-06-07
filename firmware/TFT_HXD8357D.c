//
//  Adafruit 3.5" TFT 320x480 + Touchscreen Breakout Board w/MicroSD Socket - HXD8357D
//
#include "TFT_HXD8357D.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include "SystemTime.h"
#include "SPIAsync.h"
#include "util/delay.h"

#include "Console.h"
#include "StringUtils.h"

#define MAX_PIXELS_PER_BURST 300

#define RST_OUTPORT  PORTF
#define RST_PIN      PF0
#define RST_DIR      DDRF

#define LITE_OUTPORT  PORTB
#define LITE_PIN      PB6
#define LITE_DIR      DDRB

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

typedef enum TFTState_enum {
    tfts_initial,
    tfts_waitingForPower,
    tfts_waitingForReset,
    tfts_waitingForSetc,
    tfts_waitingForSleepExit,
    tfts_waitingForDisplayOn,
    tfts_idle,
    tfts_drawingRectangle
} TFTState;
static TFTState tftState;
static SystemTime_t time;
static TFT_HXD8357D_RectangleSource rectangleSource;
static uint32_t currentRectRemainingPixels;
static uint16_t currentRectColor;
static volatile uint8_t backlightPWMCount;
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

const prog_char SETPWR1_params[] PROGMEM = {
    0x00,   // Not deep standby
    0x15,   //BT
    0x1C,   //VSPR
    0x1C,   //VSNR
    0x83,   //AP
    0xAA    //FS
};

const prog_char SETSTBA_params[] PROGMEM = {
    0x50,   //OPON normal
    0x50,   //OPON idle
    0x01,   //STBA
    0x3C,   //STBA
    0x1E,   //STBA
    0x08    //GEN
};

const prog_char SETCYC_params[] PROGMEM = {
    0x02,   //NW 0x02
    0x40,   //RTN
    0x00,   //DIV
    0x2A,   //DUM
    0x2A,   //DUM
    0x0D,   //GDON
    0x78    //GDOFF
};

const prog_char SETGAMMA_params[] PROGMEM = {
    0x02,
    0x0A,
    0x11,
    0x1d,
    0x23,
    0x35,
    0x41,
    0x4b,
    0x4b,
    0x42,
    0x3A,
    0x27,
    0x1B,
    0x08,
    0x09,
    0x03,
    0x02,
    0x0A,
    0x11,
    0x1d,
    0x23,
    0x35,
    0x41,
    0x4b,
    0x4b,
    0x42,
    0x3A,
    0x27,
    0x1B,
    0x08,
    0x09,
    0x03,
    0x00,
    0x01
};

//
//  SPI utilities
//

static void spiWrite (const uint8_t byte)
{
    SPIAsync_sendByte(byte);
    while (!SPIAsync_operationCompleted());
}

static void spiWrite16 (
    const uint16_t word,
    const uint16_t repeat)
{
    const uint8_t hi = word >> 8;
    const uint8_t lo = word & 0xFF;
    uint16_t remaining = repeat;
    while (remaining-- > 0) {
        while (!SPIAsync_operationCompleted());
        SPIAsync_sendByte(hi);
        while (!SPIAsync_operationCompleted());
        SPIAsync_sendByte(lo);
    }
}

static void spiWriteP (
   PGM_P bytes,
   const uint8_t numBytes)
{
    uint8_t remaining = numBytes;

    PGM_P bp = bytes;
    while (remaining-- != 0) {
        const uint8_t byte = pgm_read_byte(bp++);
        while (!SPIAsync_operationCompleted());
        SPIAsync_sendByte(byte);
   }
}

static uint8_t spiRead (void)
{
    while (!SPIAsync_operationCompleted()); // wait for preceding operation
    SPIAsync_requestByte();
    while (!SPIAsync_operationCompleted()); // wait for byte
    return SPIAsync_getByte();
}

//
//  end of SPI utilities
//

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

static void writeCommand (const uint8_t cmd)
{
    if (tftState > tfts_waitingForPower) {
        while (!SPIAsync_operationCompleted()); // wait for preceding operation
    }
    DC_OUTPORT &= ~(1 << DC_PIN);
    SPIAsync_sendByte(cmd);
    while (!SPIAsync_operationCompleted()); // wait for preceding operation
    DC_OUTPORT |= (1 << DC_PIN);
}

static void beginRectangle (
    const TFT_HXD8357D_Rectangle *rect)
{
    SPIAsync_assertSS();
    _delay_us(SS_SETUP_TIME);

    // set addr window
    writeCommand(HX8357_CASET); // Column addr set
    spiWrite16(rect->x, 1);          // start column
    spiWrite16(rect->x + rect->width - 1, 1);  // end column

    writeCommand(HX8357_PASET); // Row addr set
    spiWrite16(rect->y, 1);          // start row
    spiWrite16(rect->y + rect->height - 1, 1);  // end row

    writeCommand(HX8357_RAMWR); // write to RAM

    // write pixels
    currentRectColor = rect->color;
    currentRectRemainingPixels = ((uint32_t)rect->width) * rect->height;
}

// returns true if it needs to continue drawing the rectangle
static bool continueRectangle (void)
{
    const uint16_t pixelsToWrite = 
        (currentRectRemainingPixels > MAX_PIXELS_PER_BURST)
        ? MAX_PIXELS_PER_BURST
        : currentRectRemainingPixels;
    spiWrite16(currentRectColor, pixelsToWrite);

    currentRectRemainingPixels -= pixelsToWrite;
    if (currentRectRemainingPixels == 0) {
        // all pixels have been written
        while (!SPIAsync_operationCompleted());
        SPIAsync_deassertSS();
        return false;
    } else {
        return true;
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

    // make D/C pin an output, initially high
    DC_OUTPORT |= (1 << DC_PIN);
    DC_DIR  |= (1 << DC_PIN);

    SPIAsync_init(
        SPIAsync_MODE_MASTER |
        SPIAsync_SPEED_FOSC_DIV_4 |
        SPIAsync_SCK_LEAD_RISING |
        SPIAsync_SCK_LEAD_SAMPLE |
        SPIAsync_ORDER_MSB_FIRST
        );

    backlightPWMCount = 0;
    backlightBrightness = 10;
    SystemTime_registerForTickNotification(systemTimeTick);

    rectangleSource = NULL;
    tftState = tfts_initial;
}

void TFT_HXD8357D_setRectangleSource (
    TFT_HXD8357D_RectangleSource source)
{
    rectangleSource = source;
}

void TFT_HXD8357D_task (void)
{
    switch (tftState) {
        case tfts_initial:
            // let module power stabilize with RST asserted
            RST_OUTPORT &= ~(1 << RST_PIN);
            SystemTime_futureTime(20, &time);
            tftState = tfts_waitingForPower;
            break;
        case tfts_waitingForPower:
            if (SystemTime_timeHasArrived(&time)) {
                // release RST
                RST_OUTPORT |= (1 << RST_PIN);
                SPIAsync_assertSS();
                _delay_us(SS_SETUP_TIME);
                writeCommand(HX8357_SWRESET);
                SystemTime_futureTime(2, &time);
                tftState = tfts_waitingForReset;
            }
            break;
        case tfts_waitingForReset:
            if (SystemTime_timeHasArrived(&time)) {
                // reset complete
                // setextc
                writeCommand(HX8357D_SETC);
                spiWrite(0xFF);
                spiWrite(0x83);
                spiWrite(0x57);
                SystemTime_futureTime(30, &time);
                tftState = tfts_waitingForSetc;
            }
            break;
        case tfts_waitingForSetc:
            if (SystemTime_timeHasArrived(&time)) {
                // setc complete
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
                spiWriteP(SETPWR1_params, sizeof(SETPWR1_params));

                writeCommand(HX8357D_SETSTBA);  
                spiWriteP(SETSTBA_params, sizeof(SETSTBA_params));
    
                writeCommand(HX8357D_SETCYC);
                spiWriteP(SETCYC_params, sizeof(SETCYC_params));

                writeCommand(HX8357D_SETGAMMA);
                spiWriteP(SETGAMMA_params, sizeof(SETGAMMA_params));
    
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
                SystemTime_futureTime(15, &time);
                tftState = tfts_waitingForSleepExit;
            }
            break;
        case tfts_waitingForSleepExit:
            if (SystemTime_timeHasArrived(&time)) {
                // exited sleep
                writeCommand(HX8357_DISPON);  // display on
                SystemTime_futureTime(5, &time);
                tftState = tfts_waitingForDisplayOn;
            }
            break;
        case tfts_waitingForDisplayOn:
            if (SystemTime_timeHasArrived(&time)) {
                // display is on
                SPIAsync_deassertSS();
                _delay_us(SS_SETUP_TIME);
#if 0
                // did not get this to work - hung for some reason
                // get sw self test result
                DC_OUTPORT &= ~(1 << DC_PIN);
                SPIAsync_assertSS();
                _delay_us(SS_SETUP_TIME);
                SPIAsync_sendByte(HX8357_RDDSDR);
                while (!SPIAsync_operationCompleted()); // wait for preceding operation
                DC_OUTPORT |= (1 << DC_PIN);
                const uint8_t result = spiRead();
                SPIAsync_deassertSS();
                CharString_define(40, msg);
                CharString_copyP(PSTR("TFT self-test: "), &msg);
                StringUtils_appendDecimal(result, 1, 0, &msg);
                Console_printCS(&msg);
#endif
                tftState = tfts_idle;
            }
            break;
        case tfts_idle: {
            const TFT_HXD8357D_Rectangle *r =
                (rectangleSource != NULL)
                ? rectangleSource()
                : NULL;
            if (r != NULL) {
                beginRectangle(r);
                tftState = tfts_drawingRectangle;
            }
            }
            break;
        case tfts_drawingRectangle :
            if (!continueRectangle()) {
                tftState = tfts_idle;
            }
            break;
    }

}

void TFT_HXD8357D_setBacklightBrightness (
    const uint8_t brightness)
{
    backlightBrightness = brightness;
}

