#include <Arduino.h>

#include "is31fl3236.hpp"
#include "led.hpp"

const ledInd_t numLED = 72;
const ledInd_t numRows = 8;
const ledInd_t numCols = 30;

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
        spinningLED(5000, true);
        break;
    case ledFSMstates::WAVE_HORI:
        waveHorLED(3000, false);
        break;
    case ledFSMstates::WAVE_VERT:
        waveVerLED(3000, false);
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
 * \brief Paints the LEDs in each row a uniform brightness
 * 
 * \param intensities Array of intensities to paint on the rows
 * \param gamma Are intensities gamma levels or not? (0 to 63)
 * 
 * \note Bottom row is row 0
 */
void paintColumns(ledlevel_t intensities[], bool gamma) {
    if (gamma == true) {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[numRows - i]];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[0]];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[i - LEDstartIndex[2]]];
        // Top row
        for (ledInd_t i = LEDstartIndex[3]; i < numLED; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[numRows]];
    }
    else {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) LEDlevel[i] = intensities[numRows - i];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) LEDlevel[i] = intensities[0];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) LEDlevel[i] = intensities[i - LEDstartIndex[2]];
        // Top row
        for (ledInd_t i = LEDstartIndex[3]; i < numLED; i++) LEDlevel[i] = intensities[numRows];
    }
}

/**
 * \brief Paints the LEDs in each column a 
 * 
 * \param intensities Array of intensities to paint on the columns
 * \param gamma Are intensities gamma levels or not? (0 to 63)
 * 
 * \note Left column is column 0
 */
void paintRows(ledlevel_t intensities[], bool gamma) {
    if (gamma == true) {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[numCols]];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[(numCols - 1) - LEDstartIndex[1]]];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[0]];
        // Top row, since it is a bit narrower we skip the outer values
        for (ledInd_t i = LEDstartIndex[3]; i < numLED; i++) LEDlevel[i] = PWM_GAMMA_64[intensities[(i + 1) - LEDstartIndex[3]]];
    }
    else {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) LEDlevel[i] = intensities[numCols];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) LEDlevel[i] = intensities[(numCols - 1) - LEDstartIndex[1]];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) LEDlevel[i] = intensities[0];
        // Top row, since it is a bit narrower we skip the outer values
        for (ledInd_t i = LEDstartIndex[3]; i < numLED; i++) LEDlevel[i] = intensities[(i + 1) - LEDstartIndex[3]];
    }
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
void spinningLED(unsigned long periodMS, bool clockwise) {
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

/**
 * \brief Veritcal wave effect
 * 
 * \param periodMS Period for wave from end to end in milliseconds
 * \param upwards Should the wave move upwards or not
 */
void waveVerLED(unsigned long periodMS, bool upwards) {
    const ledlevel_t endIntensity = 63;
    const ledlevel_t baseIntensity = 10;
    const int incrementIntensity = (baseIntensity < endIntensity) ? 1 : -1;
    const ledlevel_t propagateLevel = 32; // Level to start next row

    static ledInd_t location = 0; // Location of leading row in effect
    static ledlevel_t rowLevels[numRows] = { 0 };
    static bool rowGrowing[numRows] = { false }; // Marks if a row's brightness is climbing or not

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();
    unsigned int stepMS = periodMS / (numRows * 2 * (incrementIntensity * (endIntensity - baseIntensity)));
    // Determine approximate time step for each lighting step so rotations are done

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    if (restart == true) {
        uniformLED(PWM_GAMMA_64[baseIntensity]);
        if (upwards) location = numRows - 1;
        else location = 0;
        return;
    }

    // Set lighting by rows
    rowGrowing[location] = true; // Always growing on the leading edge

    for (unsigned int r = 0; r < numRows; r++) {
        if (rowGrowing[r] == true) {
            // Climb to end point then start reversing
            if (rowLevels[r] != endIntensity) rowLevels[r] = rowLevels[r] + incrementIntensity;
            else rowGrowing[r] = false; // Hit endpoint
        }
        else {
            // Decend until hitting base colour
            if (rowLevels[r] != baseIntensity) rowLevels[r] = rowLevels[r] - incrementIntensity;
        } 
    }

    // Check to propagate
    if (rowLevels[location] == propagateLevel) {
        if (upwards) {
            if (location == (numRows - 1)) {
                location = 0;
            }
            else location++;
        }
        else {
            if (location == 0) {
                location = numRows - 1;
            }
            else location--;
        }
    }

    // Paint LEDs using gamma correction
    paintRows(rowLevels, true);
}

void waveHorLED(unsigned long periodMS, bool rightwards) {
    const ledlevel_t endIntensity = 63;
    const ledlevel_t baseIntensity = 10;
    const int incrementIntensity = (baseIntensity < endIntensity) ? 1 : -1;
    const ledlevel_t propagateLevel = 32; // Level to start next row

    static ledInd_t location = 0; // Location of leading row in effect
    static ledlevel_t colLevels[numCols] = { 0 };
    static bool colGrowing[numCols] = { false }; // Marks if a row's brightness is climbing or not

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();
    unsigned int stepMS = periodMS / (numCols * 2 * (incrementIntensity * (endIntensity - baseIntensity)));
    // Determine approximate time step for each lighting step so rotations are done

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    if (restart == true) {
        uniformLED(PWM_GAMMA_64[baseIntensity]);
        if (rightwards) location = numCols - 1;
        else location = 0;
        return;
    }

    // Set lighting by rows
    colGrowing[location] = true; // Always growing on the leading edge

    for (unsigned int r = 0; r < numCols; r++) {
        if (colGrowing[r] == true) {
            // Climb to end point then start reversing
            if (colLevels[r] != endIntensity) colLevels[r] = colLevels[r] + incrementIntensity;
            else colGrowing[r] = false; // Hit endpoint
        }
        else {
            // Decend until hitting base colour
            if (colLevels[r] != baseIntensity) colLevels[r] = colLevels[r] - incrementIntensity;
        } 
    }

    // Check to propagate
    if (colLevels[location] == propagateLevel) {
        if (rightwards) {
            if (location == (numCols - 1)) {
                location = 0;
            }
            else location++;
        }
        else {
            if (location == 0) {
                location = numCols - 1;
            }
            else location--;
        }
    }

    // Paint LEDs using gamma correction
    paintColumns(colLevels, true);
}