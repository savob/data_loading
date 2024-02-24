#include <Arduino.h>

#include "is31fl3236.hpp"
#include "led.hpp"

const ledInd_t numLED = 72;

ledlevel_t LEDlevel[numLED] = {0};
ledInd_t LEDstartIndex[] = {0, 8, 38, 44};
ledInd_t LEDmiddleIndex[] = {4, 23, 41, 58};
ledInd_t LEDbutton[] = {44, 42, 40, 38};

const byte PWM_GAMMA_64[64] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0b,0x0d,0x0f,0x11,0x13,0x16,
  0x1a,0x1c,0x1d,0x1f,0x22,0x25,0x28,0x2e,
  0x34,0x38,0x3c,0x40,0x44,0x48,0x4b,0x4f,
  0x55,0x5a,0x5f,0x64,0x69,0x6d,0x72,0x77,
  0x7d,0x80,0x88,0x8d,0x94,0x9a,0xa0,0xa7,
  0xac,0xb0,0xb9,0xbf,0xc6,0xcb,0xcf,0xd6,
  0xe1,0xe9,0xed,0xf1,0xf6,0xfa,0xfe,0xff
}; // Gamma levels that are perceived as even steps in brightness

/**
 * \brief Function to initialize the LEDs
 * 
 * \param drvrs Array of LED drivers
 * \note This is best called prior to the initialization the the drivers themselves
 */
void initializeLED(IS31FL3236 drvrs[]) {
    for (ledInd_t i = 0; i < numLED; i++) LEDlevel[i] = 0;

    // Configure the LED channels for each driver
    // This is done if we want to dim one set of LEDs relative to the other
    ChannelIS31FL3236 forwardsLEDs; // Settings to use for the forward facing LEDs
    ChannelIS31FL3236 sidewaysLEDs; // Settings to use for the sideways facing LEDs

    forwardsLEDs.state = true;
    forwardsLEDs.currentLimit = CurrentSettingIS31FL3236::FULL;
    sidewaysLEDs.state = true;
    sidewaysLEDs.currentLimit = CurrentSettingIS31FL3236::FULL;

    for (uint_fast8_t i = 0; i < 36; i = i + 2) {
        drvrs[0].channelConfig[i] = forwardsLEDs;
        drvrs[0].channelConfig[i + 1] = sidewaysLEDs;
        drvrs[1].channelConfig[i] = forwardsLEDs;
        drvrs[1].channelConfig[i + 1] = sidewaysLEDs;
    }
}

/**
 * \brief Maps the calculated LED values to the right hardware register
 * 
 * \param drvrs Te array of LED drivers
 * 
 * \note THIS MUST BE UPDATED WITH ANY HARDWARE CHANGES!
 * 
 * \warning This must be called so LED effects can be seen
 */
void remap(IS31FL3236 drvrs[]) {
    for (uint_fast8_t i = 0; i < 6; i++) {
        drvrs[0].duty[30 + i] = LEDlevel[i];
    }
    for (uint_fast8_t i = 6; i < 42; i++) {
        drvrs[1].duty[i - 6] = LEDlevel[i];
    }
    for (uint_fast8_t i = 42; i < 72; i++) {
        drvrs[0].duty[i - 42] = LEDlevel[i];
    }
}

/**
 * \brief Finite State Machine for the LEDs
 * 
 * \param state What state to put the LEDs into
 * \param buttons State of the buttons
 */
void LEDfsm(ledFSMstates state, uint8_t buttons) {
    static ledFSMstates prevState = ledFSMstates::SOLID;

    switch (state) {
    case ledFSMstates::BREATH:
        breathingLED(5000);
        break;
    case ledFSMstates::SPINNING:
        spinningLED(5000);
        break;
    
    default: // Solid is default case
        uniformLED(128);
        break;
    }

    prevState = state;
}

/**
 * \brief Used to check if a given effect has not been used for a while
 * 
 * \param mark The next time an effect increment is meant to occur
 * \param stepPeriod The desired period between effect increments
 * \param curTime The current time
 * 
 * \note All marameters should reference the same clock and unit (milli- or microseconds)
 * 
 * \return If a timeout/reset has occured
 */
bool checkReset(unsigned long mark, unsigned long stepPeriod, unsigned long curTime) {
    if (mark >= curTime) return false; // If mark is in the future then not timed out

    /*  Reset check

        Checking if the function hasn't been called for some time
        indirectly via a notably out of date `nextMark` value. Thus 
        the function should probably restart.
    */ 
    return (mark + (3 * stepPeriod)) < curTime;
}

/**
 * \brief Ensures an index for LEDs is valid, rolls it over if needed
 * 
 * \param ind Index to verify
 * \return Index for the LED in valid range
 */
ledInd_t constrainLEDindex(ledInd_t ind) {
    if ((ind >= 0) && (ind < numLED)) return ind; // Valid

    while (ind < 0) ind = ind + numLED; // Brings index into positives
    ind = ind % numLED; // Deal with it potentially exceeding the limit
    // Modulo after ensuring it's positive since modulo acts wierd if you use negatives

    return ind;
}

/**
 * \brief Sets all LEDs to a uniform brightness
 * 
 * \param intensity LED level to set
 */
void uniformLED(ledlevel_t intensity) {
    for (ledInd_t i = 0; i < numLED; i++) LEDlevel[i] = intensity;
}

/**
 * \brief Does a uniform cyclic breathing effect (fading in and out)
 * 
 * \param periodMS Period in ms for a complete breathing cycle
 * \note Will intrepret a lack of calls in 
 */
void breathingLED(unsigned long periodMS) {
    const ledlevel_t maxIntensity = 63;
    const ledlevel_t intensityStep = 1;

    static ledlevel_t breathingIntensity = 0;
    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    static bool climbing = true;            // True if brightness is to climb

    unsigned long currentTime = millis();

    // Determine approximate time step for each lighting step so a complete up-down cycle is done
    unsigned int stepMS = periodMS / (2 * (maxIntensity / intensityStep));

    bool restart = checkReset(nextMark, stepMS, currentTime);

    if (restart == true) {
        breathingIntensity = 0;
        climbing = true;
        nextMark = currentTime + stepMS;
    }
    else if (nextMark < currentTime) {
        nextMark = currentTime + stepMS;

        if (climbing) {
            breathingIntensity += intensityStep;
            if (breathingIntensity > maxIntensity) {
                breathingIntensity = maxIntensity;
                climbing = false;
            }
        }
        else {
            breathingIntensity -= intensityStep;
            if (breathingIntensity <= 0) {
                breathingIntensity = 0;
                climbing = true;
            }
        }
    }

    // Set all lights to gamma corrected values
    uniformLED(PWM_GAMMA_64[breathingIntensity]);
    //uniform(breathingIntensity);
}

/**
 * \brief Moves perturbations around the logo gradually
 * 
 * \param periodMS Period for one rotation around board
 * \param clockwise Direction of rotation, true for clockwise
 * \note Probably going to be pretty choppy if run slowly
 */
void spinningLED(unsigned long periodMS, bool clockwise = true) {
    const ledlevel_t backgroundIntensity = 10;
    const int numBump = 2; // Number of light "bumps" going around
    const ledInd_t spacing = numLED / numBump;
    ledlevel_t stages[] = {63, 40, 20, backgroundIntensity}; // Gamma intensities of the bumps going around
    const int numStages = sizeof(stages) / sizeof(stages[0]);
    // Need to include background to reset 

    static ledInd_t location = 0;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Determine approximate time step for each lighting step so rotations are done
    unsigned int stepMS = periodMS / numLED;

    bool restart = checkReset(nextMark, stepMS, currentTime);

    if (restart == true) {
        uniformLED(PWM_GAMMA_64[backgroundIntensity]);
        nextMark = currentTime + stepMS;
    }
    else if (nextMark < currentTime) {
        nextMark = currentTime + stepMS;

        if (clockwise) location = constrainLEDindex(location++);
        else location = constrainLEDindex(location--);

        for (int_fast8_t b = 0; b < numBump; b++) {
            ledInd_t baseAddress = location + (b * spacing);

            for (ledInd_t offset = 0; offset < numStages; offset++) {
                ledInd_t ahead = constrainLEDindex(baseAddress + offset);
                ledInd_t behind = constrainLEDindex(baseAddress - offset);
                LEDlevel[ahead] = stages[offset];
                LEDlevel[behind] = stages[offset];
            }
        }
    }
}