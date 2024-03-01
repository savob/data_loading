#ifndef LED_HEADER
#define LED_HEADER

#include <Arduino.h>
#include "is31fl3236.hpp"

typedef uint8_t ledlevel_t;
typedef int8_t ledInd_t;

/* LED Organization

LED brightness is stored in an array going clockwise from the top LED
on the right side of the logo. There are some constant to mark out 
key locations, following the same clockwise from right top scheme.
*/
extern const ledInd_t NUM_LED; // Number of LEDs lining the board
extern const ledInd_t NUM_ROW; // Number of rows the LEDs form
extern const ledInd_t NUM_COL; // Number of columns the LEDs form

extern ledlevel_t LEDlevel[];
extern ledInd_t LEDstartIndex[];
extern ledInd_t LEDmiddleIndex[];
extern ledInd_t LEDbutton[];

enum ledFSMstates {
    SOLID = 0,      // Uniform and steady glow
    BREATH,         // Uniform breathing effect
    WAVE_VERT,      // Pulse travelling vertically
    WAVE_HORI,      // Pulse travelling horizontally
    CLOUD,          // Slow gradual changes dispersed randomly along edge
    BUMPS,          // A few bumps that move along the edges randomly
    TRACKING,       // Cloud but occasionally some columns get swapped temporarily
    SPINNING,       // Slowly rotating peaks
    AUD_UNI,        // Uniform brightness based on RMS of audio
    AUD_HORI,       // Horizontal spectrum graph
    AUD_HORI_SPLIT, // Horizontal spectrum, but split left/right
    AUD_TRACKING    // Brightness left to right is interpolation of the respective RMSs
};

void initializeLED(IS31FL3236 drvrs[]);
void remapLED(IS31FL3236 drvrs[]);
void rotateLED(ledInd_t amount, bool clockwise = true);

void LEDfsm(ledFSMstates state, uint8_t buttons);

bool checkReset(unsigned long mark, unsigned long stepPeriod, unsigned long curTime);
ledInd_t constrainIndex(ledInd_t ind, ledInd_t limit = NUM_LED);
void paintColumns(ledlevel_t intensities[], bool gamma = false);
void paintRows(ledlevel_t intensities[], bool gamma = false);
void copyGammaIntoBuffer();
void uniformGamma(ledlevel_t gamma);

void breathingLED(unsigned long periodMS);
void uniformLED(ledlevel_t intensity);
void spinningLED(unsigned long periodMS, bool clockwise = true);
void waveVerLED(unsigned long periodMS, bool upwards = true);
void waveHorLED(unsigned long periodMS, bool rightwards = true);
void cloudLED(unsigned long stepMS);
void trackingLED(unsigned long stepMS, unsigned long swapDurMS = 500, unsigned int widthSwap = 3, uint8_t probOfSwap = 3);
void bumpsLED(unsigned long stepMS, uint8_t probOfStart = 3);
#endif