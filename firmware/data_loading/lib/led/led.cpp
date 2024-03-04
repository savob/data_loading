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

const byte PWM_GAMMA[] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0b,0x0d,0x0f,0x11,0x13,0x16,
  0x1a,0x1c,0x1d,0x1f,0x22,0x25,0x28,0x2e,
  0x34,0x38,0x3c,0x40,0x44,0x48,0x4b,0x4f,
  0x55,0x5a,0x5f,0x64,0x69,0x6d,0x72,0x77,
  0x7d,0x80,0x88,0x8d,0x94,0x9a,0xa0,0xa7,
  0xac,0xb0,0xb9,0xbf,0xc6,0xcb,0xcf,0xd6,
  0xe1,0xe9,0xed,0xf1,0xf6,0xfa,0xfe,0xff
}; // Gamma levels that are perceived as even steps in brightness
const unsigned int NUM_GAMMA = sizeof(PWM_GAMMA) / sizeof(PWM_GAMMA[0]);

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
 * \param buttons State of the buttons
 * \param overrideState What state to put the LEDs into if overridden
 * \param override Override state?
 */
bool LEDfsm(uint8_t buttons, double lMag[], double rMag[], double lRMS, double rRMS,
            ledFSMstates overrideState, bool override) {
    static ledFSMstates state = ledFSMstates::SOLID;
    static ledFSMstates prevState = ledFSMstates::SOLID;
    static bool invertBrightness = false;
    static bool userControl = true; // Used for user togglable setting

    bool advanceState   = ((buttons & 0b0010) != 0);
    bool returnState    = ((buttons & 0b0100) != 0);
    bool toggleInvert   = ((buttons & 0b1000) != 0);
    bool toggleUser     = ((buttons & 0b0001) != 0);

    if (toggleUser) userControl = !userControl;
    if (toggleInvert) invertBrightness = !invertBrightness;

    bool usedGamma = true;      // Keeps track of if a state used gamma levels or not
    bool sampleAudio = false;   // Keeps track of if audio sampling is needed (slows looping)
    bool allowInversion = true; // Record whether a state allows colour inversion or not

    if (override) state = overrideState;
    switch (state) {
    case ledFSMstates::BREATH:
        breathingLED(5000);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::SOLID;
        if (advanceState) state = ledFSMstates::SPINNING;
        break;
    case ledFSMstates::SPINNING:
        spinningLED(5000, userControl);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::BREATH;
        if (advanceState) state = ledFSMstates::WAVE_HORI;
        break;
    case ledFSMstates::WAVE_HORI:
        waveHorLED(3000, userControl);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::SPINNING;
        if (advanceState) state = ledFSMstates::WAVE_VERT;
        break;
    case ledFSMstates::WAVE_VERT:
        waveVerLED(3000, userControl);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::WAVE_HORI;
        if (advanceState) state = ledFSMstates::CLOUD;
        break;
    case ledFSMstates::CLOUD:
        cloudLED(8);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::WAVE_VERT;
        if (advanceState) state = ledFSMstates::TRACKING;
        break;
    case ledFSMstates::TRACKING:
        trackingLED(8, 500, 2, 5);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::CLOUD;
        if (advanceState) state = ledFSMstates::BUMPS;
        break;
    case ledFSMstates::BUMPS:
        bumpsLED(10);
        usedGamma = true;
        sampleAudio = false;
        if (returnState) state = ledFSMstates::TRACKING;
        if (advanceState) state = ledFSMstates::AUD_UNI;
        break;
    case ledFSMstates::AUD_UNI:
        audioUniformLED(10, lRMS, rRMS);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::BUMPS;
        if (advanceState) state = ledFSMstates::AUD_BALANCE;
        break;
    case ledFSMstates::AUD_BALANCE:
        audioBalanceLED(10, lRMS, rRMS);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_UNI;
        if (advanceState) state = ledFSMstates::AUD_HORI_SPECTRUM;
        break;
    case ledFSMstates::AUD_HORI_SPECTRUM:
        audioHoriSpectrumLED(10, lMag, rMag, userControl);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_BALANCE;
        if (advanceState) state = ledFSMstates::AUD_SPLIT;
        break;
    case ledFSMstates::AUD_SPLIT:
        audioSplitSpectrumLED(10, lMag, rMag, userControl);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_HORI_SPECTRUM;
        if (advanceState) state = ledFSMstates::AUD_SPLIT_SPIN;
        break;
    case ledFSMstates::AUD_SPLIT_SPIN:
        audioSplitSpectrumSpinLED(20, lMag, rMag, userControl);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_SPLIT;
        if (advanceState) state = ledFSMstates::AUD_VERT_VOL;
        break;
    case ledFSMstates::AUD_VERT_VOL:
        audioVertVolLED(20, lRMS, rRMS, userControl);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_SPLIT_SPIN;
        if (advanceState) state = ledFSMstates::AUD_HORI_VOL;
        break;
    case ledFSMstates::AUD_HORI_VOL:
        audioHoriVolLED(20, lRMS, rRMS, userControl);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_VERT_VOL;
        if (advanceState) state = ledFSMstates::AUD_HORI_SPLIT_VOL;
        break;
    case ledFSMstates::AUD_HORI_SPLIT_VOL:
        audioHoriSplitVolLED(20, lRMS, rRMS);
        usedGamma = true;
        sampleAudio = true;
        if (returnState) state = ledFSMstates::AUD_HORI_VOL;
        if (advanceState) state = ledFSMstates::SOLID;
        break;
    
    default: // Solid is default case
        static ledlevel_t level = NUM_GAMMA / 2; // Used for the solid lighting level
        if (toggleUser && (level < (NUM_GAMMA - 1))) level++;
        if (toggleInvert && (level > 0)) level--;
        uniformLED(level, true);
        usedGamma = true;
        sampleAudio = false;
        allowInversion = false; // Don't want inversion, using it for level control
        if (returnState) state = ledFSMstates::AUD_HORI_SPLIT_VOL;
        if (advanceState) state = ledFSMstates::BREATH;

        // Brightness statements for debugging
        Serial.print("Gamma / PWM:\t");
        Serial.print(level);
        Serial.print("\t");
        Serial.println(PWM_GAMMA[level]);
        delay(5);
        break;
    }

    if (prevState != state) {
        // Do we want some sort of gradual shift between states?
    }
    prevState = state;

    // Handle inverting LED brightness as needed
    if (invertBrightness && allowInversion) {
        if (usedGamma == true) {
            for (ledInd_t i = 0; i < NUM_LED; i++) LEDgamma[i] = NUM_GAMMA - LEDgamma[i];
            copyGammaIntoBuffer();
        }
        else {
            for (ledInd_t i = 0; i < NUM_LED; i++) LEDlevel[i] = 255 - LEDlevel[i];
        }
    }

    return sampleAudio; // Let the main loop if we need audio samples
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
 * \param limit Upper limit to rollover at
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
 * \param gamma Are intensities gamma levels or not? (0 to NUM_GAMMA)
 * 
 * \note Bottom row is row 0
 */
void paintRows(ledlevel_t intensities[], bool gamma) {
    ledlevel_t* targetBuf = nullptr;
    if (gamma == true) targetBuf = LEDgamma;
    else targetBuf = LEDlevel;

    // Right side
    for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++)
        targetBuf[i] = intensities[(NUM_ROW - 1) - i];
    // Bottom row
    for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++)
        targetBuf[i] = intensities[0];
    // Left side
    for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++)
        targetBuf[i] = intensities[i - LEDstartIndex[2]];
    // Top row
    for (ledInd_t i = LEDstartIndex[3]; i < NUM_LED; i++)
        targetBuf[i] = intensities[NUM_ROW - 1];
    
    if (gamma == true) copyGammaIntoBuffer();
}

/**
 * \brief Paints the LEDs in each column a uniform brightness
 * 
 * \param intensities Array of intensities to paint on the columns
 * \param gamma Are intensities gamma levels or not? (0 to NUM_GAMMA)
 * 
 * \note Left column is column 0
 */
void paintColumns(ledlevel_t intensities[], bool gamma) {
    ledlevel_t* targetBuf = nullptr;
    if (gamma == true) targetBuf = LEDgamma;
    else targetBuf = LEDlevel;

    // Right side
    for (ledInd_t i = LEDstartIndex[0]; i < LEDstartIndex[1]; i++) 
        targetBuf[i] = intensities[NUM_COL - 1];
    // Bottom row
    for (ledInd_t i = LEDstartIndex[1]; i < LEDstartIndex[2]; i++) 
        targetBuf[i] = intensities[(NUM_COL - 1) - (i - LEDstartIndex[1])];
    // Left side
    for (ledInd_t i = LEDstartIndex[2]; i < LEDstartIndex[3]; i++) 
        targetBuf[i] = intensities[0];
    // Top row, since it is a bit narrower we skip the outer values
    for (ledInd_t i = LEDstartIndex[3]; i < NUM_LED; i++) 
        targetBuf[i] = intensities[(i + 1) - LEDstartIndex[3]];


    if (gamma == true) copyGammaIntoBuffer();
}

/**
 * \brief Copies the gamma brightness buffer into the actual light intensity buffer
 * 
 */
void copyGammaIntoBuffer() {
    for (ledInd_t i = 0; i < NUM_LED; i++) {
        if (LEDgamma[i] >= NUM_GAMMA) LEDgamma[i] = NUM_GAMMA - 1;
        LEDlevel[i] = PWM_GAMMA[LEDgamma[i]];
    }
}

/**
 * \brief Sets all LEDs to a uniform brightness
 * 
 * \param intensity LED level to set
 * \param gamma Is this a gamma level (true) or direct intensity (false)
 */
void uniformLED(ledlevel_t intensity, bool gamma) {
    if (gamma == false) {
        for (ledInd_t i = 0; i < NUM_LED; i++) LEDlevel[i] = intensity;
    }
    else {
        for (ledInd_t i = 0; i < NUM_LED; i++) LEDgamma[i] = intensity;
        copyGammaIntoBuffer();
    }
}

/**
 * \brief Does a uniform cyclic breathing effect (fading in and out)
 * 
 * \param periodMS Period in ms for a complete breathing cycle
 * \note Will intrepret a lack of calls in 
 */
void breathingLED(unsigned long periodMS) {
    const ledlevel_t MAX_INTENSITY = NUM_GAMMA;
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
        uniformLED(breathingIntensity, true);
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
    uniformLED(breathingIntensity, true);
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
    ledlevel_t STAGES[] = {NUM_GAMMA, 55, 50, 45, 40, 35, 25, 20, BASE_INTENSITY}; 
    // Gamma intensities of the bumps going around
    // Need to include background to reset 
    const int NUM_STAGES = sizeof(STAGES) / sizeof(STAGES[0]);

    static ledInd_t rotation = 0;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    if (nextMark > currentTime) return; // Wait for effect mark

    // Determine approximate time step for each lighting step so rotations are done
    unsigned int stepMS = periodMS / NUM_LED;

    // Check and handle resets
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        rotation = 0;
    }

    uniformLED(BASE_INTENSITY, true);

    // Draw the bumps
    for (int_fast8_t b = 0; b < NUM_BUMPS; b++) {
        ledInd_t baseAddress = b * SPACING;

        for (ledInd_t offset = 0; offset < NUM_STAGES; offset++) {
            ledInd_t ahead = constrainIndex(baseAddress + offset);
            ledInd_t behind = constrainIndex(baseAddress - offset);
            LEDgamma[ahead] = STAGES[offset];
            LEDgamma[behind] = STAGES[offset];
        }
    }

    // Enact rotation
    rotateLED(rotation, true);
    
    // Accumulate rotations this way so direction can be seemlessly switched
    if (clockwise) rotation++;
    else rotation--;
    rotation = constrainIndex(rotation);

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

    static bool lastUpwards = false;
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
        uniformLED(START_INTENSITY, true);

        if (upwards) location = NUM_ROW - 1;
        else location = 0;

        lastUpwards = upwards;
        return;
    }

    // Deal with a direction reversal
    if (lastUpwards != upwards) {
        if (upwards) {
            for (int i = 0; i < NUM_ROW; i++) {
                if (rowGrowing[i] == true) rowGrowing[i] = false;
                else if (rowLevels[i] != START_INTENSITY) {
                    rowGrowing[i] = true;
                    location = i;
                }
            }
        }
        else {
            for (int i = NUM_ROW - 1; i >= 0; i--) {
                if (rowGrowing[i] == true) rowGrowing[i] = false;
                else if (rowLevels[i] != START_INTENSITY) {
                    rowGrowing[i] = true;
                    location = i;
                }
            }
        }
    }
    lastUpwards = upwards;

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
    // Also check for top out (happens occasionally after direction change)
    if ((rowLevels[location] == PROPAGATE_LVL) || (rowLevels[location] == END_INTENSITY)) {
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
        rowLevels[location] = rowLevels[location] + INTENSITY_INCR; // Increment new location
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
    const ledlevel_t END_INTENSITY = NUM_GAMMA;
    const ledlevel_t START_INTENSITY = 10;
    const int INTENSITY_INCR = (START_INTENSITY < END_INTENSITY) ? 1 : -1;
    const ledlevel_t PROPAGATE_LVL = 32; // Level to start next row

    static bool lastRightwards = false;
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
        uniformLED(START_INTENSITY, true);

        if (rightwards) location = NUM_COL - 1;
        else location = 0;

        lastRightwards = rightwards;
        return;
    }

    // Deal with a direction reversal
    if (lastRightwards != rightwards) {
        if (rightwards) {
            for (int i = 0; i < NUM_COL; i++) {
                if (colLevels[i] == true) colGrowing[i] = false;
                else if (colLevels[i] != START_INTENSITY) {
                    colGrowing[i] = true;
                    location = i;
                }
            }
        }
        else {
            for (int i = NUM_COL - 1; i >= 0; i--) {
                if (colLevels[i] == true) colGrowing[i] = false;
                else if (colLevels[i] != START_INTENSITY) {
                    colGrowing[i] = true;
                    location = i;
                }
            }
        }
    }
    lastRightwards = rightwards;

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
    if ((colLevels[location] == PROPAGATE_LVL) || (colLevels[location] == END_INTENSITY)) {
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
        colLevels[location] = colLevels[location] + INTENSITY_INCR; // Increment new location
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
    const ledlevel_t MAX_INCREMENT = 6;
    const unsigned int NUM_ADJUST = 12; // How many LEDs get adjusted per cycle

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {
        // Reset to middle light level
        uniformLED((MIN_INTENSITY + MAX_INTENSITY) / 2, true);
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
        ledlevel_t increment = (random() % MAX_INCREMENT) + 1;
        if (increase[i] == true) {
            if (LEDgamma[target[i]] < (MAX_INTENSITY - increment))
                LEDgamma[target[i]] = LEDgamma[target[i]] + increment;
            else LEDgamma[target[i]] = MAX_INTENSITY;
        }
        if (increase[i] == false) {
            if (LEDgamma[target[i]] > (MIN_INTENSITY + increment))
                LEDgamma[target[i]] = LEDgamma[target[i]] - increment;
            else LEDgamma[target[i]] = MIN_INTENSITY;
        }
    }
    copyGammaIntoBuffer();
}

/**
 * \brief Tracking lines effect (randomly have columns swap)
 * 
 * \param stepMS Period in milliseconds between each adjustment cycle
 * \param swapDurMS Duration a swap should last in milliseconds
 * \param widthSwap Distance of columns to be swapped
 * \param probOfSwap Likelihood of a swap per cycle out of 255
 * 
 * \note Idle effect is that of cloud but done in columns for visible swapping
 */
void trackingLED(unsigned long stepMS, unsigned long swapDurMS,
                 unsigned int widthSwap, uint8_t probOfSwap) {
    const ledlevel_t MAX_INTENSITY = 60;
    const ledlevel_t MIN_INTENSITY = 10;
    const ledlevel_t MAX_INCREMENT = 3;
    static ledlevel_t colIntensity[NUM_COL] = {0};
    const unsigned int NUM_ADJUST = 4;   // How many columns get adjusted per cycle
    const unsigned int NUM_SWAPS = 3;    // Number of possible simultanious swaps

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
        ledlevel_t increment = (random() % MAX_INCREMENT) + 1;
        if (increase[i] == true) {
            if (colIntensity[target[i]] < (MAX_INTENSITY - increment))
                colIntensity[target[i]] = colIntensity[target[i]] + increment;
            else colIntensity[target[i]] = MAX_INTENSITY;
        }
        if (increase[i] == false) {
            if (colIntensity[target[i]] > (MIN_INTENSITY + increment))
                colIntensity[target[i]] = colIntensity[target[i]] - increment;
            else colIntensity[target[i]] = MIN_INTENSITY;
        }
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

/**
 * \brief Has some bumps moving around the board edge randomly
 * 
 * \param stepMS Period in milliseconds between each adjustment cycle
 * \param probOfStart Likelihood of a swap per cycle out of 255
 */
void bumpsLED(unsigned long stepMS, uint8_t probOfStart) {
    const ledlevel_t BASE_INTENSITY = 10;
    const unsigned int NUM_BUMP = 2;                // Number of bumps
    const unsigned int MAX_MOVEMENT_PERIOD = 50;    // Maximum period between bump steps
    const unsigned int MIN_MOVEMENT_PERIOD = 10;    // Minimum period for bump steps
    const unsigned int MAX_MOVEMENT_STEP_SIZE = 1;  // The maximum number of LEDs moved per step
    const unsigned int MAX_NUMBER_OF_STEPS = 20;    // The maximum number of movement steps per motion
    ledlevel_t STAGES[] = {NUM_GAMMA, 55, 35, 20, BASE_INTENSITY}; 
    // Gamma intensities of the bumps going around
    // Need to include background to reset 
    const int NUM_STAGES = sizeof(STAGES) / sizeof(STAGES[0]);

    struct bump_t {
        unsigned long moveTime = 0; // When to move again (based on `millis()` time)
        unsigned int movePeriod = 0;
        ledInd_t location = 0;      // Location of bump center
        ledInd_t numKeyPoints = 0;
        ledlevel_t* keypoints;      // Points to array for key stages
        int motionIncrement = 0;    // Size of each step (counterclockwise if negative)
        int stepsRemaining = 0;
    };
    static struct bump_t bump[NUM_BUMP];

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;

    // Reset to base
    uniformLED(BASE_INTENSITY, true);

    // Handle potential reset
    bool restart = checkReset(nextMark, stepMS, currentTime);
    nextMark = currentTime + stepMS; // Update mark after reset check
    if (restart == true) {

        // Reset bumps
        const ledInd_t SPACING = NUM_LED / NUM_BUMP;
        for (unsigned int i = 0; i < NUM_BUMP; i++) {
            bump[i].location = i * SPACING;
            bump[i].stepsRemaining = 0;
            bump[i].keypoints = STAGES;
            bump[i].numKeyPoints = NUM_STAGES;
            bump[i].moveTime = 0;
            bump[i].movePeriod = MAX_MOVEMENT_PERIOD;
            bump[i].motionIncrement = 0; // Clockwise if true
        }

        // Do not return, continue to execute
    }

    // Set up the bumps
    for (unsigned int i = 0; i < NUM_BUMP; i++) {
        // Bump on the move?
        if (bump[i].stepsRemaining > 0) {
            if (currentTime > bump[i].moveTime) {
                // Bump on the move!
                bump[i].location = bump[i].location + bump[i].motionIncrement;
                bump[i].location = constrainIndex(bump[i].location);

                bump[i].stepsRemaining--;
                bump[i].moveTime = currentTime + bump[i].movePeriod;
            }
            continue; // Skip to next bump
        }

        // Stationary bump, will it move?
        unsigned long roll = random();
        if ((roll & 0xFF) >= probOfStart) continue; // Not moving, go to next bump

        // Set up new motion
        roll = random();
        bump[i].stepsRemaining = 1 + (roll & 0xFFUL) % MAX_NUMBER_OF_STEPS;

        // Decide on a motion, occasionally negative
        bump[i].motionIncrement = 1 + (roll & 0xFF00UL) % MAX_MOVEMENT_STEP_SIZE;
        if ((roll & 0x10000UL) == 0) bump[i].motionIncrement = -bump[i].motionIncrement;

        bump[i].movePeriod = (roll & 0xFFFE0000UL) % (MAX_MOVEMENT_PERIOD - MIN_MOVEMENT_PERIOD);
        bump[i].movePeriod = MIN_MOVEMENT_PERIOD + bump[i].movePeriod;
        bump[i].moveTime = currentTime + bump[i].movePeriod;
    }

    // Render the bumps
    // Need to be careful to not overwrite bumps incorrectly when in proximity
    // Want to preserve the information/brightness of the closest bump
    for (unsigned int i = 0; i < NUM_BUMP; i++) {
        LEDgamma[bump[i].location] = bump[i].keypoints[0]; // Always paint crest of a bump

        // Extend out in a direction unless it encounter another bump that will be dominant
        bool isDomForwards = true;
        bool isDomBackwards = true;
        for (ledInd_t offset = 1; offset < NUM_STAGES; offset++) {
            ledInd_t tarIndFwd = constrainIndex(bump[i].location + offset);
            ledInd_t tarIndBack = constrainIndex(bump[i].location - offset);

            // Scan along a second offset from the current offset to compare to other bumps
            for (int so = 0; so < offset; so++) {
                for (unsigned int c = 0; c < NUM_BUMP; c++) {
                    ledInd_t doubOffsetFwd = constrainIndex(tarIndFwd + so);
                    ledInd_t doubOffsetBack = constrainIndex(tarIndFwd - so);
                    if (bump[c].location == doubOffsetFwd) isDomForwards = false;
                    if (bump[c].location == doubOffsetBack) isDomBackwards = false;
                }
            }

            // Paint only if still dominant in that direction
            if (isDomForwards) LEDgamma[tarIndFwd] = STAGES[offset];
            if (isDomBackwards) LEDgamma[tarIndBack] = STAGES[offset];
        }
    }

    copyGammaIntoBuffer();
}

/**
 * \brief Uniform illumination based on overall sound RMS
 * 
 * \param stepMS Time between updates
 * \param leftRMS RMS of left audio channel (should be between 0 and 1)
 * \param rightRMS RMS of right audio channel (should be between 0 and 1)
 * 
 * \note Although this doesn't truely need to be paced, it's included to pace updates to lighting chips
 */
void audioUniformLED(unsigned long stepMS, double leftRMS, double rightRMS) {
    const double SCALING = 7.0;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Determine overall RMS, clamp it to 1 at max
    double overall = leftRMS * leftRMS + rightRMS * rightRMS;
    overall = SCALING * sqrt(overall / 2.0);
    if (overall >= 1.0) overall = 0.999;

    ledlevel_t level = overall * NUM_GAMMA;

    uniformLED(level, true);
}

/**
 * \brief Tracks the audio balancing left to right
 * 
 * \param stepMS Time between updates
 * \param leftRMS RMS of left audio channel (should be between 0 and 1)
 * \param rightRMS RMS of right audio channel (should be between 0 and 1)
 * 
 * \note Need to work on this, it isn't too nice or notable right now.
 * \note Hence why there are all the `Serial.print()`s left in.
 */
void audioBalanceLED(unsigned long stepMS, double leftRMS, double rightRMS) {
    const double SCALING = 7.0;

    // Scale and clamp intensities
    leftRMS = leftRMS * SCALING;
    rightRMS = rightRMS * SCALING;

    // Serial.print(leftRMS, 3);
    // Serial.print(" ");
    // Serial.print(rightRMS, 3);
    // Serial.print(" | ");

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Perform level rule to interpolate values between edges
    ledlevel_t colMag[NUM_COL];
    const float GRADIENT = (rightRMS - leftRMS) / (float)(NUM_COL - 1);
    for (int i = 0; i < NUM_COL; i++) {
        float temp;
        temp = (GRADIENT * i) + leftRMS;

        if (temp >= 1.0) temp = 0.999; // Clamp

        colMag[i] = temp * NUM_GAMMA;
        // Serial.print(colMag[i], 5);
        // Serial.print("\t");
    }
    // Serial.println("");
    paintColumns(colMag, true);
}

/**
 * \brief Trims the spectrum response to cover half the LEDs
 * 
 * \param lIn Left audio spectrum
 * \param rIn Right audio spectrum
 * \param lOut Array to record trimmed spectrum for left channel
 * \param rOut Array to record trimmed spectrum for right channel
 * 
 * \note Input arrays are assumed to be of size `NUM_SPECTRUM`
 * \note Output arrays are assumed to be of size `NUM_LED / 2`
 */
void filterSpectrum(double lIn[], double rIn[], double lOut[], double rOut[]) {
    /* Focus is on trimming quick and easy

       Currently focusing on the lower portion of the spectrum since our perception is logarithmic.
       Then using alternating samples for the remainder since we can't use alternating samples for
       the entire trimmed spectrum.

       This scheme is basically inadmissable for any meaningful processing but that's not what
       we're looking for here. Just pretty lights.
    */
    const int LOWER_END = 8;
    
    for (int i = 0; i < LOWER_END; i++) {
        lOut[i] = lIn[i];
        rOut[i] = rIn[i];
    }
    for (int i = LOWER_END; i < (NUM_LED / 2); i++) {
        int newIndex;
        newIndex = (i - LOWER_END) * 2;
        newIndex = newIndex + LOWER_END;

        lOut[i] = lIn[newIndex];
        rOut[i] = rIn[newIndex];
    }
}

/**
 * \brief Horizontal spectrum graph across the entire board
 * 
 * \param stepMS Time between updates (ms)
 * \param left Left spectrum magnitudes
 * \param right Right spectrum magnitudes
 * \param leftToRight Should the lowest frequencies start at the left (true) or right
 */
void audioHoriSpectrumLED(unsigned long stepMS, double left[], double right[], bool leftToRight) {
    const double SCALING = NUM_GAMMA / 2; // Spectrum levels are clamped to 1

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Acquire the trimmed response and combine into one array
    double lRes[NUM_LED / 2], rRes[NUM_LED / 2]; 
    filterSpectrum(left, right, lRes, rRes);
    for (int i = 0; i < (NUM_LED / 2); i++) {
        lRes[i] = lRes[i] + rRes[i];
    }

    // Get the levels for the graph
    // This will lose some of the higher frequency marks, oh well
    ledlevel_t columns[NUM_COL];
    for (int i = 0; i < NUM_COL; i++) {
        lRes[i] = lRes[i] * SCALING;
        
        if (leftToRight) columns[i] = lRes[i];
        else columns[NUM_COL - (i + 1)] = lRes[i];
    }

    paintColumns(columns, true);
}

/**
 * \brief Shows a split spectrum for each channel
 * 
 * \param stepMS Time between updates (ms)
 * \param left Left spectrum magnitudes
 * \param right Right spectrum magnitudes
 * \param bottomToTop Should the spectrum start with the lowest frequencies at the bottom (true) or not
 */
void audioSplitSpectrumLED(unsigned long stepMS, double left[], double right[], bool bottomToTop) {
    const double SCALING = NUM_GAMMA;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Acquire the trimmed response and combine into one array
    double lRes[NUM_LED / 2], rRes[NUM_LED / 2]; 
    filterSpectrum(left, right, lRes, rRes);

    int baseLocation = 0;
    if (bottomToTop) baseLocation = LEDmiddleIndex[1];
    else baseLocation = LEDmiddleIndex[3];

    // Get the levels for the graph
    for (int i = 0; i < (NUM_LED / 2); i++) {
        ledInd_t curLeft, curRight;
        if (bottomToTop) {
            curLeft = constrainIndex(baseLocation + i + 1);
            curRight = constrainIndex(baseLocation - i);
        }
        else {
            curRight = constrainIndex(baseLocation + i + 1);
            curLeft = constrainIndex(baseLocation - i);
        }
        LEDgamma[curLeft] = lRes[i] * SCALING;
        LEDgamma[curRight] = rRes[i] * SCALING;
    }
    copyGammaIntoBuffer();
}

/**
 * \brief Shows a split spectrum for each channel but gradually rotating around
 * 
 * \param stepMS Time between updates (ms)
 * \param left Left spectrum magnitudes
 * \param right Right spectrum magnitudes
 * \param clockwise Should the spectrum start with the lowest frequencies at the bottom (true) or not
 */
void audioSplitSpectrumSpinLED(unsigned long stepMS, double left[], double right[], bool clockwise) {

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Just run the normal split and then rotate
    static int rotation = 0;
    audioSplitSpectrumLED(stepMS, left, right, true);
    rotateLED(rotation, true);

    // Accumulate rotations this way so rotation can be seemlessly switched
    if (clockwise) rotation++;
    else rotation--;
    rotation = constrainIndex(rotation);
}

/**
 * \brief Vertical volume bar efect
 * 
 * \param stepMS Time between updates
 * \param leftRMS RMS of left audio channel (should be between 0 and 1)
 * \param rightRMS RMS of right audio channel (should be between 0 and 1)
 * \param bottomToTop Paint volume from bottom (true) or top
 */
void audioVertVolLED(unsigned long stepMS, double leftRMS, double rightRMS, bool bottomToTop) {
    const double SCALING = 8.0;
    const unsigned int FALLDOWN_PERIOD = 200;
    const ledlevel_t PEAK_INTENSITY = 63;
    const ledlevel_t BASE_INTENSITY = 10;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Calculate overall RMS
    double overallRMS = 0;
    overallRMS = leftRMS * leftRMS + rightRMS * rightRMS;
    overallRMS = sqrt(overallRMS / 2.0);
    if (overallRMS > 1.0) overallRMS = 1.0;

    // Calculate volume
    double partialRow = overallRMS * NUM_ROW * SCALING;
    if (partialRow > NUM_ROW) partialRow = NUM_ROW;
    ledInd_t fullRow = partialRow;
    partialRow = partialRow - fullRow; // Get remainder

    // Order into columns
    ledlevel_t rows[NUM_ROW];
    for (ledInd_t i = 0; i < NUM_ROW; i++) rows[i] = BASE_INTENSITY;
    for (ledInd_t i = 0; i < fullRow; i++) rows[i] = PEAK_INTENSITY;
    rows[fullRow] = (PEAK_INTENSITY - BASE_INTENSITY) * partialRow;

    // Upper mark, falls at a set rate
    static unsigned long nextPeakMark = 0;
    static ledInd_t peakLocation = NUM_ROW -1;

    if (nextPeakMark < currentTime) {
        nextPeakMark = currentTime + FALLDOWN_PERIOD;

        if (peakLocation > 0) peakLocation--;
    }
    else if (fullRow >= peakLocation) {
        peakLocation = fullRow + 1;
        nextPeakMark = currentTime + FALLDOWN_PERIOD;
    }
    rows[peakLocation] = PEAK_INTENSITY;

    // Reverse if needed
    if (bottomToTop == false) {
        ledlevel_t temp[NUM_ROW];
        for (int i = 0; i < NUM_ROW; i++) temp[i] = rows[NUM_ROW - (1 + i)];
        for (int i = 0; i < NUM_ROW; i++) rows[i] = temp[i];
    }

    paintRows(rows, true);
}

/**
 * \brief Horizontal volume bar effect
 * 
 * \param stepMS Time between updates
 * \param leftRMS RMS of left audio channel (should be between 0 and 1)
 * \param rightRMS RMS of right audio channel (should be between 0 and 1)
 * \param leftToRight Should the bar go from the left (true) or right?
 */
void audioHoriVolLED(unsigned long stepMS, double leftRMS, double rightRMS, bool leftToRight) {
    const double SCALING = 8.0;
    const unsigned int FALLDOWN_PERIOD = 100;
    const ledlevel_t PEAK_INTENSITY = 63;
    const ledlevel_t BASE_INTENSITY = 10;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Calculate overall RMS
    double overallRMS = 0;
    overallRMS = leftRMS * leftRMS + rightRMS * rightRMS;
    overallRMS = sqrt(overallRMS / 2.0);
    if (overallRMS > 1.0) overallRMS = 1.0;

    // Calculate volume
    double partialCol = overallRMS * NUM_COL * SCALING;
    if (partialCol > NUM_COL) partialCol = NUM_COL;
    ledInd_t fullCol = partialCol;
    partialCol = partialCol - fullCol; // Get remainder

    // Order into columns
    ledlevel_t cols[NUM_COL];
    for (ledInd_t i = 0; i < NUM_COL; i++) cols[i] = BASE_INTENSITY;
    for (ledInd_t i = 0; i < fullCol; i++) cols[i] = PEAK_INTENSITY;
    cols[fullCol] = (PEAK_INTENSITY - BASE_INTENSITY) * partialCol;

    // Upper mark, falls at a set rate
    static unsigned long nextPeakMark = 0;
    static ledInd_t peakLocation = NUM_COL - 1;

    if (nextPeakMark < currentTime) {
        nextPeakMark = currentTime + FALLDOWN_PERIOD;

        if (peakLocation > 0) peakLocation--;
    }
    else if (fullCol >= peakLocation) {
        peakLocation = fullCol + 1;
        nextPeakMark = currentTime + FALLDOWN_PERIOD;
    }
    cols[peakLocation] = PEAK_INTENSITY;

    // Reverse if needed
    if (leftToRight == false) {
        ledlevel_t temp[NUM_COL];
        for (int i = 0; i < NUM_COL; i++) temp[i] = cols[NUM_COL - (1 + i)];
        for (int i = 0; i < NUM_COL; i++) cols[i] = temp[i];
    }

    paintColumns(cols, true);
}

/**
 * \brief Horizontal split volume for channels
 * 
 * \param stepMS Time between updates
 * \param leftRMS RMS of left audio channel (should be between 0 and 1)
 * \param rightRMS RMS of right audio channel (should be between 0 and 1)
 */
void audioHoriSplitVolLED(unsigned long stepMS, double leftRMS, double rightRMS) {
    const double SCALING = 4.0;
    const unsigned int FALLDOWN_PERIOD = 150;
    const ledlevel_t PEAK_INTENSITY = 63;
    const ledlevel_t BASE_INTENSITY = 10;

    static unsigned long nextMark = 0;      // Marks next time to adjust brightness
    unsigned long currentTime = millis();

    // Check if it is time to adjust effects or not
    if (nextMark > currentTime) return;
    nextMark = currentTime + stepMS;
    // There's no need to handle resets since this is a instantanious effect

    // Calculate volumes
    double partialCol[2];
    partialCol[0] = leftRMS * NUM_COL * SCALING;
    partialCol[1] = rightRMS * NUM_COL * SCALING;

    ledlevel_t cols[NUM_COL];
    for (ledInd_t i = 0; i < NUM_COL; i++) cols[i] = BASE_INTENSITY;

    for (int i = 0; i < 2; i++) {

        if (partialCol[i] > NUM_COL) partialCol[i] = NUM_COL;
        ledInd_t fullCol = partialCol[i];
        partialCol[i] = partialCol[i] - fullCol; // Get remainder

        // Order into columns
        if (i == 0) {
            // Left side
            const ledInd_t BASE = NUM_COL / 2 - 1;
            for (ledInd_t i = 0; i < fullCol; i++) cols[BASE - i] = PEAK_INTENSITY;
            cols[BASE - fullCol] = (PEAK_INTENSITY - BASE_INTENSITY) * partialCol[i];
        }
        else {
            // Right
            const ledInd_t BASE = NUM_COL / 2;
            for (ledInd_t i = 0; i < fullCol; i++) cols[i + BASE] = PEAK_INTENSITY;
            cols[fullCol + BASE + 1] = (PEAK_INTENSITY - BASE_INTENSITY) * partialCol[i];
        }

        // Upper mark, falls at a set rate
        static unsigned long nextPeakMark[2] = {0};
        static ledInd_t peakLocation[2] = {0};

        if (nextPeakMark[i] < currentTime) {
            nextPeakMark[i] = currentTime + FALLDOWN_PERIOD;

            if (i == 0) {
                if (peakLocation[i] < (NUM_COL / 2)) peakLocation[i]++;

                if ((NUM_COL / 2) - fullCol < peakLocation[i]) {
                    peakLocation[i] = (NUM_COL / 2) - fullCol;
                    nextPeakMark[i] = currentTime + FALLDOWN_PERIOD;
                }
            }
            else {
                if (peakLocation[i] > ((NUM_COL / 2) + 1)) peakLocation[i]--;

                if (fullCol + (NUM_COL / 2) + 1 >= peakLocation[i]) {
                    peakLocation[i] = fullCol + (NUM_COL / 2) + 1;
                    nextPeakMark[i] = currentTime + FALLDOWN_PERIOD;
                }
            }
        }
        cols[peakLocation[i]] = PEAK_INTENSITY;
    }

    paintColumns(cols, true);
}