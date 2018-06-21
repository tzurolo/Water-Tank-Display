//
//  Font definitions
//

#include "DisplayFonts.h"

#include "FreeSans12pt7b.h"

#include "Console.h"
#include "StringUtils.h"

#if 0
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
#endif

const GFXfont* DisplayFonts_primary (void)
{
    return &FreeSans12pt7b;
}

uint8_t DisplayFonts_fontHeight (
    const GFXfont* font)
{
    const int8_t yBottom = pgm_read_byte(&font->yBottom);
    const int8_t yTop = pgm_read_byte(&font->yTop);

    return yBottom - yTop;
}
