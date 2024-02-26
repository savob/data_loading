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
void remap(IS31FL3236 drvrs[]);

void LEDfsm(ledFSMstates state, uint8_t buttons);

bool checkReset(unsigned long mark, unsigned long stepPeriod, unsigned long curTime);
ledInd_t constrainLEDindex(ledInd_t ind);
void paintColumns(ledlevel_t intensities[], bool gamma = false);
void paintRows(ledlevel_t intensities[], bool gamma = false);
void copyGammaIntoBuffer();
void uniformGamma(ledlevel_t gamma);

void breathingLED(unsigned long periodMS);
void uniformLED(ledlevel_t intensity);
void spinningLED(unsigned long periodMS, bool clockwise);
void waveVerLED(unsigned long periodMS, bool upwards);
void waveHorLED(unsigned long periodMS, bool rightwards);
void cloudLED(unsigned long stepMS);
#endif