//
//  SD Card
//

#include "SDCard.h"

#include <avr/io.h>

#define CARDCS_DDR        DDRF
#define CARDCS_INPORT     PINF
#define CARDCS_OUTPORT    PORTF
#define CARDCS_PIN        PF7

#define CARDDETECT_DDR        DDRF
#define CARDDETECT_INPORT     PINF
#define CARDDETECT_OUTPORT    PORTF
#define CARDDETECT_PIN        PF6

void SDCard_Initialize (void)
{
    // set up CS pin as outpu, set to high (unasserted) 
    CARDCS_OUTPORT |= (1 << CARDCS_PIN);
    CARDCS_DDR |= (1 << CARDCS_PIN);

    // set up card detect pin as input, turn on pull-up
    CARDDETECT_DDR &= (~(1 << CARDDETECT_PIN));
    CARDDETECT_OUTPORT |= (1 << CARDDETECT_PIN);
}

bool SDCard_isPresent (void)
{
    return ((CARDDETECT_INPORT & (1 << CARDDETECT_PIN)) != 0);
}

