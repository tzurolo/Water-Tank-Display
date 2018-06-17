//
//  Font definitions
//

#ifndef DISPLAYFONTS_H
#define DISPLAYFONTS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "gfxfont.h"

extern const GFXfont* DisplayFonts_primary (void);

extern uint8_t DisplayFonts_fontHeight (
    const GFXfont* font);

#endif  /* DISPLAYFONTS_H */

