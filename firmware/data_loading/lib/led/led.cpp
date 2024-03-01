#include <Arduino.h>

#include "is31fl3236.hpp"
#include "led.hpp"

const ledInd_t NUM_LED = 72;
const ledInd_t NUM_ROW = 8;
const ledInd_t NUM_COL = 30;

ledlevel_t LEDlevel[NUM_LED] = {0}; // Actual LED brightness
ledlevel_t LEDgamma[NUM_LED] = {0}; // Used to store LED gamma levels. Must be copied into actual buffer.
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
    for (ledInd_t i = 0; i < NUM_LED; i++) LEDlevel[i] = 0;

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
 * \param drvrs The array of LED drivers
 * 
 * \note THIS MUST BE UPDATED WITH ANY HARDWARE CHANGES!
 * 
 * \warning This must be called so LED effects can be seen properly
 */
void remapLED(IS31FL3236 drvrs[]) {
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
 * \brief Rotates the LED brightness buffers
 * 
 * \param amount Number of places to rotate LEDs by
 * \param clockwise Direction of rotation to take
 */
void rotateLED(ledInd_t amount, bool clockwise) {
    amount = constrainIndex(amount); // Limits rotation to under one rotation
    if (amount == 0) return; // Return if no rotation is needed

    ledlevel_t tempLevel[NUM_LED] = {0};
    ledlevel_t tempGamma[NUM_LED] = {0};
    for (ledInd_t i = 0; i < NUM_LED; i++) {
        tempLevel[i] = LEDlevel[i];
        tempGamma[i] = LEDgamma[i];
    }

    if (clockwise) {
        for (ledInd_t i = 0; i < NUM_LED; i++) {
            LEDlevel[i] = tempLevel[constrainIndex(i - amount)];
            LEDgamma[i] = tempGamma[constrainIndex(i - amount)];
        }
    }
    else {
        for (ledInd_t i = 0; i < NUM_LED; i++) {
            LEDlevel[i] = tempLevel[constrainIndex(i + amount)];
            LEDgamma[i] = tempGamma[constrainIndex(i + amount)];
        }
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

    if (prevState != state) {
        // Do we want some sort of gradual shift between states?
    }

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
    case ledFSMstates::CLOUD:
        cloudLED(10);
        break;
    case ledFSMstates::TRACKING:
        trackingLED(10, 500, 2, 3);
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

        Checking if the function hasn't been called for some time indirectly via a 
        notably out of date `nextMark` value. Thus the function should probably restart.

        It also accounts for audio sampling by using the larger of the two periods:
        function step and approximate audio sampling period.
    */ 
    const unsigned long AUDIO_SAMPLE_PERIOD = 40;
    const unsigned long FUNC_TIMEOUT_PERIOD = 3 * stepPeriod;

    bool timedOut;
    if (AUDIO_SAMPLE_PERIOD > FUNC_TIMEOUT_PERIOD) timedOut = (mark + AUDIO_SAMPLE_PERIOD) < curTime;
    else timedOut = (mark + FUNC_TIMEOUT_PERIOD) < curTime;
    if (timedOut) return true;

    return (mark == 0); // If mark is zero then it's the first time a function's run, should reset
}

/**
 * \brief Ensures an index for LEDs is valid, rolls it over if needed
 * 
 * \param ind Index to verify
 * \param limit Upper limit to rollover at (default is number of LEDs)
 * 
 * \return Index in the valid range
 */
ledInd_t constrainIndex(ledInd_t ind, ledInd_t limit) {
    if ((ind >= 0) && (ind < limit)) return ind; // Valid

    while (ind < 0) ind = ind + limit; // Brings index into positives
    ind = ind % limit; // Deal with it potentially exceeding the limit
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
void paintRows(ledlevel_t intensities[], bool gamma) {
    if (gamma == true) {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[NUM_ROW - i]];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[0]];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[i - LEDstartIndex[2]]];
        // Top row
        for (ledInd_t i = LEDstartIndex[3]; i < NUM_LED; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[NUM_ROW - 1]];
    }
    else {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) 
            LEDlevel[i] = intensities[NUM_ROW - i];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) 
            LEDlevel[i] = intensities[0];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) 
            LEDlevel[i] = intensities[i - LEDstartIndex[2]];
        // Top row
        for (ledInd_t i = LEDstartIndex[3]; i < NUM_LED; i++) 
            LEDlevel[i] = intensities[NUM_ROW - 1];
    }
}

/**
 * \brief Paints the LEDs in each column a uniform brightness
 * 
 * \param intensities Array of intensities to paint on the columns
 * \param gamma Are intensities gamma levels or not? (0 to 63)
 * 
 * \note Left column is column 0
 */
void paintColumns(ledlevel_t intensities[], bool gamma) {
    if (gamma == true) {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[NUM_COL - 1]];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[(NUM_COL - 1) - (i - LEDstartIndex[1])]];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[0]];
        // Top row, since it is a bit narrower we skip the outer values
        for (ledInd_t i = LEDstartIndex[3]; i < NUM_LED; i++) 
            LEDlevel[i] = PWM_GAMMA_64[intensities[(i + 1) - LEDstartIndex[3]]];
    }
    else {
        // Right side
        for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) 
            LEDlevel[i] = intensities[NUM_COL - 1];
        // Bottom row
        for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) 
            LEDlevel[i] = intensities[(NUM_COL - 1) - (i - LEDstartIndex[1])];
        // Left side
        for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) 
            LEDlevel[i] = intensities[0];
        // Top row, since it is a bit narrower we skip the outer values
        for (ledInd_t i = LEDstartIndex[3]; i < NUM_LED; i++) 
            LEDlevel[i] = intensities[(i + 1) - LEDstartIndex[3]];
    }
}

/**
 * \brief Copies the gamma brightness buffer into the actual light intensity buffer
 * 
 */
void copyGammaIntoBuffer() {
    for (ledInd_t i = 0; i < NUM_LED; i++) LEDlevel[i] = PWM_GAMMA_64[LEDgamma[i]];
}

/**
 * \brief Sets LEDs to a uniform gamma level
 * 
 * \param gamma Gamma level
 */
void uniformGamma(ledlevel_t gamma) {
    for (ledInd_t i = 0; i < NUM_LED; i++) LEDgamma[i] = gamma;
    copyGammaIntoBuffer();
}

/**
 * \brief Sets all LEDs to a uniform brightness
 * 
 * \param intensity LED level to set
 */
void uniformLED(ledlevel_t intensity) {
    for (ledInd_t i = 0; i < NUM_LED; i++) LEDlevel[i] = intensity;
}

/**
 * \brief Does a uniform cyclic breathing effect (fading in and out)
 * 
 * \param periodMS Period in ms for a complete breathing cycle
 * \note Will intrepret a lack of calls in 
 */
void breathingLED(unsigned long periodMS) {
    const ledlevel_t MAX_INTENSITY = 63;
    const ledlevel_t MIN_INTENSITY = 0;
    const ledlevel_t intensityStep = 1;

    static ledlevel_t breathingIntensity = 0;
    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    static bool climbing = true;            // True if brightness is to climb
    unsigned long currentTime = millis();

    if (nextMark > currentTime) return; // Wait for effect mark

    // Determine approximate time step for each lighting step so a complete up-down cycle is done
    unsigned int stepMS = periodMS / (2 * (MAX_INTENSITY / intensityStep));

    // Check and handle resets
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        breathingIntensity = MIN_INTENSITY;
        climbing = true;
        uniformGamma(breathingIntensity);
        return;
    }

    // Actually enact effect
    if (climbing) {
        if (breathingIntensity >= MAX_INTENSITY) {
            breathingIntensity = MAX_INTENSITY;
            climbing = false;
        }
        else breathingIntensity += intensityStep;
    }
    else {
        if (breathingIntensity <= MIN_INTENSITY) {
            breathingIntensity = MIN_INTENSITY;
            climbing = true;
        }
        else breathingIntensity -= intensityStep;
    }
    uniformGamma(breathingIntensity);
}

/**
 * \brief Moves perturbations around the logo gradually
 * 
 * \param periodMS Period for one rotation around board
 * \param clockwise Direction of rotation, true for clockwise
 * \note Probably going to be pretty choppy if run slowly
 */
void spinningLED(unsigned long periodMS, bool clockwise) {
    const ledlevel_t BASE_INTENSITY = 10;
    const int NUM_BUMPS = 2; // Number of light "bumps" going around
    const ledInd_t SPACING = NUM_LED / NUM_BUMPS;
    ledlevel_t STAGES[] = {63, 55, 50, 45, 40, 35, 25, 20, BASE_INTENSITY}; 
    // Gamma intensities of the bumps going around
    const int NUM_STAGES = sizeof(STAGES) / sizeof(STAGES[0]);
    // Need to include background to reset 

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    if (nextMark > currentTime) return; // Wait for effect mark

    // Determine approximate time step for each lighting step so rotations are done
    unsigned int stepMS = periodMS / NUM_LED;

    // Check and handle resets
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        uniformGamma(BASE_INTENSITY);

        // Draw the bumps after reset (they will just be rotated)
        for (int_fast8_t b = 0; b < NUM_BUMPS; b++) {
            ledInd_t baseAddress = b * SPACING;

            for (ledInd_t offset = 0; offset < NUM_STAGES; offset++) {
                ledInd_t ahead = constrainIndex(baseAddress + offset);
                ledInd_t behind = constrainIndex(baseAddress - offset);
                LEDgamma[ahead] = STAGES[offset];
                LEDgamma[behind] = STAGES[offset];
            }
        }
        return;
    }

    // Enact rotation
    rotateLED(1, clockwise);
    copyGammaIntoBuffer();
}

/**
 * \brief Vertical wave effect
 * 
 * \param periodMS Period for wave from end to end in milliseconds
 * \param upwards Should the wave move upwards or not
 */
void waveVerLED(unsigned long periodMS, bool upwards) {
    const ledlevel_t END_INTENSITY = 60;
    const ledlevel_t START_INTENSITY = 10;
    const int INTENSITY_INCR = (START_INTENSITY < END_INTENSITY) ? 1 : -1;
    const ledlevel_t PROPAGATE_LVL = 30; // Level to start next row

    static ledInd_t location = 0; // Location of leading row in effect
    static ledlevel_t rowLevels[NUM_ROW] = { 0 };
    static bool rowGrowing[NUM_ROW] = { false }; // Marks if a row's brightness is climbing or not

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();
    unsigned int stepMS = periodMS / (NUM_ROW * 2 * ((END_INTENSITY - START_INTENSITY) /  INTENSITY_INCR));
    // Determine approximate time step for each lighting step so rotations are done

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        for (ledInd_t i = 0; i < NUM_ROW; i++) rowLevels[i] = START_INTENSITY;
        uniformLED(PWM_GAMMA_64[START_INTENSITY]);

        if (upwards) location = NUM_ROW - 1;
        else location = 0;
        return;
    }

    // Set lighting by rows
    rowGrowing[location] = true; // Always growing on the leading edge

    for (unsigned int r = 0; r < NUM_ROW; r++) {
        if (rowGrowing[r] == true) {
            // Climb to end point then start reversing
            if (rowLevels[r] != END_INTENSITY) rowLevels[r] = rowLevels[r] + INTENSITY_INCR;
            else rowGrowing[r] = false; // Hit endpoint
        }
        else {
            // Decend until hitting base colour
            if (rowLevels[r] != START_INTENSITY) rowLevels[r] = rowLevels[r] - INTENSITY_INCR;
        } 
    }

    // Check to propagate
    if (rowLevels[location] == PROPAGATE_LVL) {
        if (upwards) {
            if (location == (NUM_ROW - 1)) {
                location = 0;
            }
            else location++;
        }
        else {
            if (location == 0) {
                location = NUM_ROW - 1;
            }
            else location--;
        }
    }

    // Paint LEDs using gamma correction
    paintRows(rowLevels, true);
}

/**
 * \brief Horizontal wave effect
 * 
 * \param periodMS Period for wave from end to end in milliseconds
 * \param rightwards Should the wave scroll rightwards or not
 */
void waveHorLED(unsigned long periodMS, bool rightwards) {
    const ledlevel_t END_INTENSITY = 63;
    const ledlevel_t START_INTENSITY = 10;
    const int INTENSITY_INCR = (START_INTENSITY < END_INTENSITY) ? 1 : -1;
    const ledlevel_t PROPAGATE_LVL = 32; // Level to start next row

    static ledInd_t location = 0; // Location of leading row in effect
    static ledlevel_t colLevels[NUM_COL] = { 0 };
    static bool colGrowing[NUM_COL] = { false }; // Marks if a row's brightness is climbing or not

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();
    unsigned int stepMS = periodMS / (NUM_COL * 2 * ((END_INTENSITY - START_INTENSITY) / INTENSITY_INCR));
    // Determine approximate time step for each lighting step so rotations are done

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        for (ledInd_t i = 0; i < NUM_COL; i++) colLevels[i] = START_INTENSITY;
        uniformLED(PWM_GAMMA_64[START_INTENSITY]);

        if (rightwards) location = NUM_COL - 1;
        else location = 0;
        return;
    }

    // Set lighting by rows
    colGrowing[location] = true; // Always growing on the leading edge

    for (unsigned int r = 0; r < NUM_COL; r++) {
        if (colGrowing[r] == true) {
            // Climb to end point then start reversing
            if (colLevels[r] != END_INTENSITY) colLevels[r] = colLevels[r] + INTENSITY_INCR;
            else colGrowing[r] = false; // Hit endpoint
        }
        else {
            // Decend until hitting base colour
            if (colLevels[r] != START_INTENSITY) colLevels[r] = colLevels[r] - INTENSITY_INCR;
        } 
    }

    // Check to propagate
    if (colLevels[location] == PROPAGATE_LVL) {
        if (rightwards) {
            if (location == (NUM_COL - 1)) {
                location = 0;
            }
            else location++;
        }
        else {
            if (location == 0) {
                location = NUM_COL - 1;
            }
            else location--;
        }
    }

    // Paint LEDs using gamma correction
    paintColumns(colLevels, true);
}

/**
 * \brief Randomly adjusts brightness around entire perimeter
 * \note Kind of like a lava lamp
 * 
 * \param stepMS Period in milliseconds between each adjustment cycle
 */
void cloudLED(unsigned long stepMS) {
    const ledlevel_t MAX_INTENSITY = 60;
    const ledlevel_t MIN_INTENSITY = 10;
    const ledlevel_t INCREMENT = 1;
    const unsigned int NUM_ADJUST = 8; // How many LEDs get adjusted per cycle

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        // Reset to middle light level
        uniformGamma((MIN_INTENSITY + MAX_INTENSITY) / 2);
        return;
    }

    // Come up with the adjustments to make
    bool increase[NUM_ADJUST] = {false};
    ledInd_t target[NUM_ADJUST] = {0};

    for (uint_fast8_t i = 0; i < NUM_ADJUST; i++) {

        bool uniqueChange = true;
        do {
            // Need to use entirely independant bits for each part of a change to avoid correlations
            unsigned long temp = random();
            increase[i] = temp & 1;
            target[i] = constrainIndex(temp >> 1); // Discard direction bit for location calculation

            uniqueChange = true;
            for (uint_fast8_t c = 0; c < i; c++) {
                if (target[c] == target[i]) uniqueChange = false;
            }
        } while (uniqueChange == false);
    }
    
    // Enact the changes if valid
    for (uint_fast8_t i = 0; i < NUM_ADJUST; i++) {
        if ((increase[i] == true) && (LEDgamma[target[i]] < MAX_INTENSITY)) 
            LEDgamma[target[i]] = LEDgamma[i] + INCREMENT;
        if ((increase[i] == false) && (LEDgamma[target[i]] > MIN_INTENSITY)) 
            LEDgamma[target[i]] = LEDgamma[i] - INCREMENT;;
    }
    copyGammaIntoBuffer();
}

/**
 * \brief Tracking lines effect (randomly have columns swap)
 * 
 * \param stepMS Period in milliseconds between each adjustment cycle
 * \param swapDurMS Duration a swap should last in milliseconds (default is 500)
 * \param widthSwap Distance of columns to be swapped (default is 3)
 * \param probOfSwap Likelihood of a swap per cycle out of 255 (default is 3)
 * 
 * \note Idle effect is that of cloud but done in columns for visible swapping
 */
void trackingLED(unsigned long stepMS, unsigned long swapDurMS,
                 unsigned int widthSwap, uint8_t probOfSwap) {
    const ledlevel_t MAX_INTENSITY = 60;
    const ledlevel_t MIN_INTENSITY = 10;
    const ledlevel_t INCREMENT = 3;
    static ledlevel_t colIntensity[NUM_COL] = {0};
    const unsigned int NUM_ADJUST = 2;   // How many columns get adjusted per cycle
    const unsigned int NUM_SWAPS = 2;    // Number of possible simultanious swaps

    struct swap_t {
        bool enabled = false;       // Is this swap active?
        unsigned long endTime = 0;  // When to deactivate (based on `millis()` time)
        ledInd_t location = 0;      // What is the start of this swap
    };
    static swap_t swaps[NUM_SWAPS];

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        // Reset to middle light level
        for (ledInd_t i = 0; i < NUM_COL; i++) colIntensity[i] = (MIN_INTENSITY + MAX_INTENSITY) / 2;
        paintColumns(colIntensity);

        // Reset swaps
        for (unsigned int i = 0; i < NUM_SWAPS; i++) swaps[i].enabled = false;
        return;
    }

    // Come up with the adjustments to make
    bool increase[NUM_ADJUST] = {false};
    ledInd_t target[NUM_ADJUST] = {0};

    for (uint_fast8_t i = 0; i < NUM_ADJUST; i++) {

        bool uniqueChange = true;
        do {
            // Need to use entirely independant bits for each part of a change to avoid correlations
            unsigned long temp = random();
            increase[i] = temp & 1;
            target[i] = constrainIndex((temp >> 1), NUM_COL); // Discard direction bit for location calculation

            uniqueChange = true;
            for (uint_fast8_t c = 0; c < i; c++) {
                if (target[c] == target[i]) uniqueChange = false;
            }
        } while (uniqueChange == false);
    }
    
    // Enact the changes if valid
    for (uint_fast8_t i = 0; i < NUM_ADJUST; i++) {
        if ((increase[i] == true) && (colIntensity[target[i]] < MAX_INTENSITY)) 
            colIntensity[target[i]] = colIntensity[target[i]] + INCREMENT;
        if ((increase[i] == false) && (colIntensity[target[i]] > MIN_INTENSITY)) 
            colIntensity[target[i]] = colIntensity[target[i]] - INCREMENT;
    }

    // Work through swaps
    for (unsigned int i = 0; i < NUM_SWAPS; i++) {
        if (swaps[i].enabled == true) {
            if (currentTime > swaps[i].endTime) {
                swaps[i].enabled = false;

                // Undo the swap
                ledlevel_t temp = colIntensity[swaps[i].location];
                colIntensity[swaps[i].location] = 
                    colIntensity[constrainIndex(swaps[i].location + widthSwap, NUM_COL)];
                colIntensity[constrainIndex(swaps[i].location + widthSwap, NUM_COL)] = temp;
            }
        }
        else {
            // Check if it should swap
            unsigned long roll = random();
            swaps[i].enabled = (roll & 0xFF) < probOfSwap;

            if (!swaps[i].enabled) continue;

            swaps[i].endTime = currentTime + swapDurMS;

            // Generate a swap that doesn't overlap another currently active swap
            bool uniqueSwap = true;
            do {
                roll = random();
                swaps[i].location = constrainIndex(roll, NUM_COL);

                uniqueSwap = true;
                for (uint_fast8_t c = 0; c < NUM_SWAPS; c++) {
                    if (swaps[c].enabled == false) continue;
                    if (c == i) continue; // Don't compare to itself
                    if (swaps[i].location == swaps[c].location) uniqueSwap = false;
                    if (swaps[i].location == constrainIndex(swaps[c].location + widthSwap, NUM_COL)) 
                        uniqueSwap = false;
                }
            } while (uniqueSwap == false);

            // Perform the swap
            ledlevel_t temp = colIntensity[swaps[i].location];
            colIntensity[swaps[i].location] = colIntensity[constrainIndex(swaps[i].location + widthSwap, NUM_COL)];
            colIntensity[constrainIndex(swaps[i].location + widthSwap, NUM_COL)] = temp;
        }
    }

    paintColumns(colIntensity, true);
}