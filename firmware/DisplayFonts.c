//
//  Font definitions
//

#include "DisplayFonts.h"

#include "FreeSans12pt7b.h"

// special characters
const uint16_t powerIcon[] PROGMEM = {
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


const GFXfont* DisplayFonts_primary (void)
{
    return &FreeSans12pt7b;
}
