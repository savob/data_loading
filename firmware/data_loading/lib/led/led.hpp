#ifndef LED_HEADER
#define LED_HEADER

#include <Arduino.h>
#include "is31fl3236.hpp"

/* LED Organization

LED brightness is stored in an array going clockwise from the top LED
on the right side of the logo. There are some constant to mark out 
key locations, following the same clockwise from right top scheme.
*/
extern uint8_t LEDlevel[];
extern uint8_t LEDstartIndex[];
extern uint8_t LEDmiddleIndex[];
extern uint8_t LEDbutton[];

enum ledFSMstates {
    BREATH      = 0,
    SOLID       = 1
};

void initializeLED(IS31FL3236 drvrs[]);
void remap(IS31FL3236 drvrs[]);

void LEDfsm(ledFSMstates state, uint8_t buttons);

void breathingLED(unsigned int stepMS);
void uniformLED(uint8_t intensity);
#endif