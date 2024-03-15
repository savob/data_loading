#include <Arduino.h>
#include <Wire.h>
#include "cap1206.hpp"

const int CAP1206_TRANSFER_FAIL = -1;
const int CAP1206_TRANSFER_SUCCESS = 0;

uint8_t defaultThresholds[] = {40, 40, 40, 40, 10, 10};

/**
 * \brief Construct a new CAP1206 object
 * 
 * \param bus I2C bus to find the sensor on
 */
Cap1206::Cap1206(TwoWire* bus) {
    interface = bus;
}

/**
 * \brief Writes a value to a single register
 * 
 * \param reg Target register on the CAP1206
 * \param val Value to write
 * \return Return status of the transfer 
 */
int Cap1206::writeSingleReg(RegistersCap1206 reg, uint8_t val) {
    uint8_t count = 0;

    interface->beginTransmission(ADDRESS_CAP); 
    count = interface->write(reg);
    count = count + interface->write(val);

    // Check if both fields were communicated correctly
    if ((count == 2) && (interface->endTransmission() == 0)) return CAP1206_TRANSFER_SUCCESS;
    return CAP1206_TRANSFER_FAIL;
}

/**
 * \brief Reads the value of a given register on the CAP1206
 * 
 * \param reg Target register
 * \param tar Address to record register value in
 * \return Return status of the transfer 
 */
int Cap1206::readSingleReg(RegistersCap1206 reg, uint8_t* tar) {
    return readManyRegs(reg, 1, tar);
}

/**
 * \brief Performs a block read of the registers on the CAP1206
 * 
 * \param reg Starting address of block read
 * \param num Number of registers to read
 * \param tar Address to record register values into
 * 
 * \note This will read continuously through the memory, potentially into invalid regions. 
 * \note Know what you're looking for!
 * 
 * \return Return status of the transfer 
 */
int Cap1206::readManyRegs(RegistersCap1206 reg, uint8_t num, uint8_t* tar) {
    uint8_t count = 0;

    // Send address, then draw data
    interface->beginTransmission(ADDRESS_CAP); 
    count = interface->write(reg);
    if ((interface->endTransmission(false) != 0) || (count != 1)) return CAP1206_TRANSFER_FAIL; 
    // End message but not with stop

    interface->beginTransmission(ADDRESS_CAP); 
    count = interface->requestFrom(ADDRESS_CAP, num);
    if ((interface->endTransmission() != 0) || (count != num)) return CAP1206_TRANSFER_FAIL;

    // Copy if all data was communicated correctly
    for (uint_fast8_t i = 0; i < num; i++) {
        tar[i] = interface->read();
    }
    return CAP1206_TRANSFER_SUCCESS;
}

/**
 * \brief Sets the main control register
 * 
 * \param stby Put the chip into standby
 * \param dslp Put the chip into deep sleep (overrides standby)
 * \param clrInt Clear the interrupt flag
 * \return Return status of the transfer 
 */
int Cap1206::setMainControl(bool stby, bool dslp, bool clrInt) {
    uint8_t temp = 0;

    standbyEn = stby;
    deepSleepEn = dslp;

    if (stby == true) temp |= 0x20;
    if (dslp == true) temp |= 0x10;
    if (clrInt == false) temp |= 0x01;

    return writeSingleReg(RegistersCap1206::MAIN_CTRL, temp);
}

/**
 * \brief Checks the current interrupt state of the chip
 * 
 * \param tar Location to record if an interrupt has been flagged
 * \return Return status of the transfer 
 */
int Cap1206::checkInterrupt(bool* tar) {
    uint8_t temp = 0;

    if (checkMainControl(&temp) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;

    if ((temp & 0x01) == 0x01) *tar = true;
    else *tar = false;

    return CAP1206_TRANSFER_SUCCESS;
}

/**
 * \brief Clears interrupt flag while respecting previous sleep states
 * 
 * \return Return status of the transfer 
 */
int Cap1206::clearInterrupt() {
    return setMainControl(standbyEn, deepSleepEn, true);
}

/**
 * \brief Returns the state of the main control register
 * 
 * \param tar Location to record state
 * \return Return status of the transfer 
 */
int Cap1206::checkMainControl(uint8_t* tar) {
    return readSingleReg(RegistersCap1206::MAIN_CTRL, tar);
}

/**
 * \brief Retrieve general status register
 * 
 * \param tar Location to record state
 * \return Return status of the transfer 
 */
int Cap1206::checkGenStatus(uint8_t* tar) {
    return readSingleReg(RegistersCap1206::GEN_STATUS, tar);
}

/**
 * \brief Retrieve general status register flags
 * 
 * \param bc Pointer for base bount exceeding limit
 * \param acal Pointer for analog calibration fails
 * \param pwr Pointer for power button pressing
 * \param mult Pointer for multitouch being active
 * \param mtp Pointer for contacts exceeding multitouch count
 * \param touch Pointer for touch detection
 * 
 * \note If not interested in any given flags just have a shared "trash" pointer as their arguement.
 * \return Return status of the transfer 
 */
int Cap1206::checkGenStatusFlags(bool* bc, bool* acal, bool* pwr, bool*mult, bool*mtp, bool* touch) {
    uint8_t temp = 0;

    if (checkGenStatus(&temp) == CAP1206_TRANSFER_FAIL) {
        *bc     = false;
        *acal   = false;
        *pwr    = false;
        *mult   = false;
        *mtp    = false;
        *touch  = false;

        return CAP1206_TRANSFER_FAIL;
    }

    *bc     = (temp & (1 << 6)) != 0;
    *acal   = (temp & (1 << 5)) != 0;
    *pwr    = (temp & (1 << 4)) != 0;
    *mult   = (temp & (1 << 2)) != 0;
    *mtp    = (temp & (1 << 1)) != 0;
    *touch  = (temp & (1 << 0)) != 0;

    return CAP1206_TRANSFER_SUCCESS;
};

/**
 * \brief Reads sensor states to an array
 * 
 * \param target Array of booleans to write state to (must be at least 6 elements)
 * \return Return status of the transfer 
 */
int Cap1206::readSensors(bool target[]) {
    uint8_t temp = 0;

    if (readSensors(&temp) == CAP1206_TRANSFER_FAIL) {
        for (uint_fast8_t i = 0; i < 6; i++) target[i] = false;
        return CAP1206_TRANSFER_FAIL;
    }

    for (uint_fast8_t i = 0; i < 6; i++) target[i] = (temp & (1 << i)) != 0;
    return CAP1206_TRANSFER_SUCCESS;
}

/**
 * \brief Retrieve the sensor state register value
 * 
 * \param target Location to record state to
 * \return Return status of the transfer 
 */
int Cap1206::readSensors(uint8_t* target) {

    // Checks if an interrupt for input was raised
    bool interruptFound = false;
    if (checkInterrupt(&interruptFound) != CAP1206_TRANSFER_SUCCESS) return CAP1206_TRANSFER_FAIL;
    
    // If there's no interrupt then no input to process
    if (interruptFound == false) *target = 0;
    else {
        if(readSingleReg(RegistersCap1206::SENSOR_INPUT, target) != CAP1206_TRANSFER_SUCCESS) 
            return CAP1206_TRANSFER_FAIL;
    }

    // if (*target != 0) SerialUSB.println(*target, BIN); // Output which button is pressed

    // Need to clear interrupt to reset button states for next check
    // Needed to not register releases
    return clearInterrupt();
}

/**
 * \brief Reads noise flag states to an array
 * 
 * \param target Array of booleans to write state to (must be at least 6 elements)
 * \return Return status of the transfer 
 */
int Cap1206::readNoiseFlags(bool target[]) {
    uint8_t temp = 0;

    if (readNoiseFlags(&temp) == CAP1206_TRANSFER_FAIL) {
        for (uint_fast8_t i = 0; i < 6; i++) target[i] = false;
        return CAP1206_TRANSFER_FAIL;
    }

    for (uint_fast8_t i = 0; i < 6; i++) target[i] = (temp & (1 << i)) != 0;
    return CAP1206_TRANSFER_SUCCESS;
}

/**
 * \brief Retrieve the noise flag register value
 * 
 * \param target Location to record state to
 * \return Return status of the transfer 
 */
int Cap1206::readNoiseFlags(uint8_t* target) {
    return readSingleReg(RegistersCap1206::NOISE_FLAG, target);
}

/**
 * \brief Adjust the sensitivity of touch detection
 * 
 * \param sens Sensitivity level to use
 * \param shift Scaling of the base count registers
 * 
 * \note Base count shifting doesn't affect detection or sensitivity to touches, so no need to adjust
 * \return Return status of the transfer 
 */
int Cap1206::setSensitivity(DeltaSensitivityCap1206 sens, BaseShiftCap1206 shift) {
    uint8_t temp = 0;

    temp |= sens << 4;
    temp |= shift;

    return writeSingleReg(RegistersCap1206::SENS_CTRL, temp);
}

/**
 * \brief Set configuration register 1
 * 
 * \param smbTO Enable the timeout functionality of the SMBus protocol
 * \param disDigNoise Disables the digital noise ignoring feature
 * \param disAnaNoise Disables the analog noise ignoring feature
 * \param maxDurEn Enables auto recalibration if a button is held for too long
 * \return Return status of the transfer 
 */
int Cap1206::setConfig1(bool smbTO, bool disDigNoise, bool disAnaNoise, bool maxDurEn) {
    uint8_t temp = 0;

    if (smbTO == true)          temp |= 0x80;
    if (disDigNoise == true)    temp |= 0x02;
    if (disAnaNoise == true)    temp |= 0x01;
    if (maxDurEn == true)       temp |= 0x04;

    return writeSingleReg(RegistersCap1206::CONFIG_1, temp);
}

/**
 * \brief Set configuration register 2
 * 
 * \param bcOutRecal Controls whether to retry analog calibration when the base count is out of limit on a sensor
 * \param powReduction Reduce power when waiting between conversion finishing and sensing cycle finishing
 * \param bcOutInt Trigger an interrupt when base count is out of limit
 * \param showRFnoiseOnly Show only RF noise as the noise flag for sensors
 * \param disRFnoise Disable the RF filtering feature for sensors
 * \param anaCalFailInt Trigger an interrupt if an analog calibration fails
 * \param intRelease Trigger interrupts when buttons are released
 * \return Return status of the transfer 
 */
int Cap1206::setConfig2(bool bcOutRecal, bool powReduction, bool bcOutInt, bool showRFnoiseOnly, 
        bool disRFnoise, bool anaCalFailInt, bool intRelease) {
    uint8_t temp = 0;

    if (bcOutRecal == true)         temp |= 0x40;
    if (powReduction == true)       temp |= 0x20;
    if (bcOutInt == true)           temp |= 0x10;
    if (showRFnoiseOnly == true)    temp |= 0x08;
    if (disRFnoise == true)         temp |= 0x04;
    if (anaCalFailInt == true)      temp |= 0x02;
    if (intRelease == false)        temp |= 0x01;

    return writeSingleReg(RegistersCap1206::CONFIG_2, temp);
}

/**
 * \brief Enable selected sensors on the CAP1206 
 * 
 * \param sensors Array of which buttons to enable. Must be of size six. 
 * \return Return status of the transfer 
 */
int Cap1206::enableSensors(bool sensors[]) {
    uint8_t temp = 0;

    for (uint_fast8_t i = 0; i < 6; i++) {
        if (sensors[i] == true) temp |= (1 << i);
    }

    return enableSensors(temp);
}

/**
 * \brief Enable selected sensors on the CAP1206  
 * 
 * \param sensors Mask of which buttons to enable 
 * \return Return status of the transfer 
 */
int Cap1206::enableSensors(uint8_t sensors) {
    return writeSingleReg(RegistersCap1206::SENS_IN_EN, sensors);
}

/**
 * \brief Enable repeat events on selected sensors on the CAP1206 
 * 
 * \param sensors Array of which buttons to enable. Must be of size six. 
 * \return Return status of the transfer 
 */
int Cap1206::enableRepeat(bool sensors[]) {
    uint8_t temp = 0;

    for (uint_fast8_t i = 0; i < 6; i++) {
        if (sensors[i] == true) temp |= (1 << i);
    }

    return enableRepeat(temp);
}

/**
 * \brief Enable repeat events on selected sensors on the CAP1206  
 * 
 * \param sensors Mask of which buttons to enable 
 * \return Return status of the transfer 
 */
int Cap1206::enableRepeat(uint8_t sensors) {
    return writeSingleReg(RegistersCap1206::REPEAT_RATE_EN, sensors);
}

/**
 * \brief Set the sensor input configuration 1 register
 * 
 * \param dur Maximum duration of touch allowed until recalibrated automatically
 * \param rep Time between interrupts when auto repeat is enabled
 * \return Return status of the transfer 
 */
int Cap1206::setSensorInputConfig1(MaxDurationcap1206 dur, RepeatRateCap1206 rep) {
    uint8_t temp = 0;

    temp |= rep;
    temp |= (dur << 4);

    return writeSingleReg(RegistersCap1206::SENS_IN_CONF_1, temp);
}

/**
 * \brief Set the sensor input configuration 2 register
 * 
 * \param min Minium time before prior to auto repeat starts
 * \return Return status of the transfer 
 */
int Cap1206::setSensorInputConfig2(MinForRepeatCap1206 min) {
    uint8_t temp = 0;

    temp |= min;

    return writeSingleReg(RegistersCap1206::SENS_IN_CONF_2, temp);
}

/**
 * \brief Configure how measurements are taken
 * 
 * \param ave Number of samples taken per measurement
 * \param sam Time to take a single sample, affects magnitude of base counts
 * \param cyc Desired sensing cycle time (period)
 * \return Return status of the transfer 
 */
int Cap1206::setAverageAndSampling(AveragedSamplesCap1206 ave, SampleTimeCap1206 sam, CycleTime1206 cyc) {
    uint8_t temp = 0;

    temp |= cyc;
    temp |= (sam << 2);
    temp |= (ave << 4);

    return writeSingleReg(RegistersCap1206::AVE_SAMP_CONF, temp);
}

/**
 * \brief Sets the noise threshold for the CAP1206
 * 
 * \param thrs Noise threshold to use for inputs
 * \return Return status of the transfer 
 */
int Cap1206::setSensorInputNoiseThreshold(SensNoiseThrsCap1206 thrs) {
    uint8_t temp = 0;

    temp |= thrs;

    return writeSingleReg(RegistersCap1206::SENS_NOISE_THRS, temp);
}

/**
 * \brief Trigger calibration on selected sensors on the CAP1206 
 * 
 * \param sensors Array of which buttons to calibrate. Must be of size six. 
 * \return Return status of the transfer 
 */
int Cap1206::setCalibrations(bool sensors[]) {
    uint8_t temp = 0;

    for (uint_fast8_t i = 0; i < 6; i++) {
        if (sensors[i] == true) temp |= (1 << i);
    }

    return enableRepeat(temp);
}

/**
 * \brief Trigger calibration on selected sensors on the CAP1206
 * 
 * \param sensors Mask of which buttons to calibrate 
 * \return Return status of the transfer 
 */
int Cap1206::setCalibrations(uint8_t sensors) {
    return writeSingleReg(RegistersCap1206::CALIB_ACT_STAT, sensors);
}

/**
 * \brief Reads calibration flag states to an array
 * 
 * \param sensors Array of booleans to write state to (must be at least 6 elements)
 * \return Return status of the transfer 
 */
int Cap1206::readCalibrations(bool sensors[]) {
    uint8_t temp = 0;

    if (readCalibrations(&temp) == CAP1206_TRANSFER_FAIL) {
        for (uint_fast8_t i = 0; i < 6; i++) sensors[i] = true; // Assume all are under calibration
        return CAP1206_TRANSFER_FAIL;
    }

    for (uint_fast8_t i = 0; i < 6; i++) sensors[i] = (temp & (1 << i)) != 0;
    return CAP1206_TRANSFER_SUCCESS;
}

/**
 * \brief Retrieve the calibration flag register value
 * 
 * \param target Location to record state to
 * \return Return status of the transfer 
 */
int Cap1206::readCalibrations(uint8_t* sensors) {
    return readSingleReg(RegistersCap1206::CALIB_ACT_STAT, sensors);
}

/**
 * \brief Enable interrupt events on selected sensors on the CAP1206 
 * 
 * \param sensors Array of which buttons to calibrate. Must be of size six. 
 * \return Return status of the transfer 
 */
int Cap1206::enableInterrupt(bool sensors[]) {
    uint8_t temp = 0;

    for (uint_fast8_t i = 0; i < 6; i++) {
        if (sensors[i] == true) temp |= (1 << i);
    }

    return enableInterrupt(temp);
}

/**
 * \brief Enable interrupt events on selected sensors on the CAP1206
 * 
 * \param sensors Mask of which buttons to enable interrupts on 
 * \return Return status of the transfer 
 */
int Cap1206::enableInterrupt(uint8_t sensors) {
    return writeSingleReg(RegistersCap1206::INT_EN, sensors);
}

/**
 * \brief Configure the behaviour of automatic recalibrations
 * 
 * \param ldth Enable setting all sensor threshold by writing to sensor 1's threshold
 * \note Other sensors threshold can still be set individually even if all are set by sensor 1's value 
 * \param clrint Clear intermediate acumulated data if noise is detected
 * \param clrneg Clear the consecutive negative delta count if noise is detected
 * \note `clrint` and `clrneg` should be set to the same value per datasheet
 * \param negcnt Number of consecutive negative deltas to trigger a digital recalibration
 * \param cal Update time and samples needed for automatic recalibrations
 * \return Return status of the transfer 
 */
int Cap1206::setRecalConfig(bool ldth, bool clrint, bool clrneg, 
        NegDeltaCountCap1206 negcnt, CalConfigCap1206 cal) {
    uint8_t temp = 0;

    if (ldth == true)       temp |= (1 << 7);
    if (clrint == false)    temp |= (1 << 6);
    if (clrneg == false)    temp |= (1 << 5); 
    temp |= (negcnt << 3);
    temp |= cal;

    return writeSingleReg(RegistersCap1206::RECAL_CONF, temp);
}

/**
 * \brief Sets the threshold for a given button
 * 
 * \param but Button index to adjust
 * \note Buttons are indexed from 0 in this code but the data sheet starts at 1
 * \param thres New threshold for the button (0 - 127 inclusive, will be clamped)
 * \note Setting the value for button 1 may overwrite the other buttons' thresholds. 
 * \note Thus button 1 should generally be set first and then the others.
 * 
 * \return Return status of the transfer 
 */
int Cap1206::setButtonThreshold(uint8_t but, uint8_t thres) {
    if (thres > 127) thres = 127;

    switch (but) {
    case 1:
        return writeSingleReg(RegistersCap1206::SENS_THRS_2, thres);
    case 2:
        return writeSingleReg(RegistersCap1206::SENS_THRS_3, thres);
    case 3:
        return writeSingleReg(RegistersCap1206::SENS_THRS_4, thres);
    case 4:
        return writeSingleReg(RegistersCap1206::SENS_THRS_5, thres);
    case 5:
        return writeSingleReg(RegistersCap1206::SENS_THRS_6, thres);
    default: // Default to writing for button 1
        return writeSingleReg(RegistersCap1206::SENS_THRS_1, thres);
    }
}

/**
 * \brief Reads the delta count register for a given button
 * 
 * \param target Location to record the delta count
 * \note Buttons are indexed from 0 in this code but the data sheet starts at 1
 * \param but Index of the button of interest
 * \return Return status of the transfer 
 */
int Cap1206::readDelta(int8_t* target, uint8_t but) {
    switch (but) {
    case 1:
        return readSingleReg(RegistersCap1206::DELTA_2, (uint8_t*) target);
    case 2:
        return readSingleReg(RegistersCap1206::DELTA_3, (uint8_t*) target);
    case 3:
        return readSingleReg(RegistersCap1206::DELTA_4, (uint8_t*) target);
    case 4:
        return readSingleReg(RegistersCap1206::DELTA_5, (uint8_t*) target);
    case 5:
        return readSingleReg(RegistersCap1206::DELTA_6, (uint8_t*) target);
    default: // Default to reading for button 1
        return readSingleReg(RegistersCap1206::DELTA_1, (uint8_t*) target);
    }
}

/**
 * \brief Set the multiple touch detection configuration
 * 
 * \param en Enable or disable the multiple touch detection system
 * \param blockNum Number of simultaneous touches before blocking, from 1 to 4
 * \return Return status of the transfer 
 */
int Cap1206::setMultiTouchConfig(bool en, uint8_t blockNum) {
    uint8_t temp = 0;

    if (en == true) temp |= (1 << 7);

    switch (blockNum) {
    case 2:
        temp |= (1 << 3);
        break;
    case 3:
        temp |= (2 << 3);
        break;
    case 4:
        temp |= (3 << 3);
        break;
    default: // Default to 1 button blocking (as is device default)
        temp |= (0 << 3);
        break;
    }

    return writeSingleReg(RegistersCap1206::MUL_TOUCH_CONF, temp);
}

/**
 * \brief Updates all the button thresholds
 * 
 * \param thres Array of six thresholds for the button thresholds (0 - 127 inclusive, will be clamped).
 * \note Buttons are indexed from 0 in this code but the data sheet starts at 1
 * \return Return status of the transfer 
 */
int Cap1206::setButtonThresholds(uint8_t thres[]) {
    uint8_t count = 0;

    interface->beginTransmission(ADDRESS_CAP);
    // Use a sequential (block) write to update them quickly starting at button 1
    count = interface->write(RegistersCap1206::SENS_THRS_1);

    for (uint_fast8_t i = 0; i < 6; i++) {
        if (thres[i] > 127) thres[i] = 127;
        count = count + interface->write(thres[i]);
    }
    interface->endTransmission();

    // Check if all the data was communicated correctly
    if ((count == 7) && (interface->endTransmission() == 0)) return CAP1206_TRANSFER_SUCCESS;
    return CAP1206_TRANSFER_FAIL;
}

/**
 * \brief Reads the ID from the CAP1206 chip
 * 
 * \param id Location to store the returned ID, should be 0x67
 * \return Return status of the transfer 
 */
int Cap1206::readProductID(uint8_t* id) {
    return readSingleReg(RegistersCap1206::PROD_ID, id);
}

/**
 * \brief Reads the manufacturer ID from the CAP1206 chip
 * 
 * \param id Location to store the returned ID, should be 0x5D
 * \return Return status of the transfer 
 */
int Cap1206::readManufacturerID(uint8_t* id) {
    return readSingleReg(RegistersCap1206::MANU_ID, id);
}

/**
 * \brief Returns the chip revision for the CAP1206
 * 
 * \param rev Location to store the returned revision number
 * \return Return status of the transfer 
 */
int Cap1206::readRevision(uint8_t* rev) {
    return readSingleReg(RegistersCap1206::REV, rev);
}

/**
 * \brief Initializes the CAP1206 sensor
 * 
 * \return Return status of the transfer 
 */
int Cap1206::initialize() {

    interface->begin(); // Begin I2C bus
    interface->setClock(400000);

    // The initial configuration is largely chip defaults

    if (setMainControl(false,   // Put chip into standby
                       false,   // Put chip into deep sleep
                       true)    // Clear interrupt flag
                       == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setSensitivity(DeltaSensitivityCap1206::MUL_002, 
                       BaseShiftCap1206::SCALE_256) 
                       == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setConfig1(false,   // SMBus timeout enable
                   false,   // Disable digital noise ignoring feature
                   false,   // Disable analog noise ignoring feature
                   true)    // Auto recalibrate button if held for too long
                   == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setConfig2(true,    // Base count out of limits recal
                   true,    // Power reduction 
                   false,   // Base count limit interrupt
                   false,   // Show noise flag only for RF noise
                   false,   // Disable RF noise detection
                   false,   // Analog calibration failure interrupt
                   false)   // Interupt on button release
                   == CAP1206_TRANSFER_FAIL)
        return CAP1206_TRANSFER_FAIL;
    
    // Only enabling the tabs buttons (no 'DA' nor 'TA' due to sensitivity issues)
    if (enableSensors((uint8_t)(0x0F)) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (enableRepeat((uint8_t)(0x0F)) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    
    if (setSensorInputConfig1(MaxDurationcap1206::MAX_DUR_05600, 
                              RepeatRateCap1206::REP_RATE_035) 
                              == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setSensorInputConfig2(MinForRepeatCap1206::MIN_PER_560) == CAP1206_TRANSFER_FAIL) 
        return CAP1206_TRANSFER_FAIL;
    if (setAverageAndSampling(AveragedSamplesCap1206::SMPL_008, 
                              SampleTimeCap1206::US_1280, 
                              CycleTime1206::MS_035) 
                              == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setSensorInputNoiseThreshold(SensNoiseThrsCap1206::PER_375) == CAP1206_TRANSFER_FAIL)
        return CAP1206_TRANSFER_FAIL;
    
    // Calibrate all sensors on start
    if (setCalibrations((uint8_t)(0x0F)) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL; 
    if (enableInterrupt((uint8_t)(0x0F)) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setRecalConfig(false,   // Set all threshold by writing to sensor one threshold
                       false,   // Clear intermediate data if noise detected
                       false,   // Clear negative count if noise detected
                       NegDeltaCountCap1206::COUNT_16, 
                       CalConfigCap1206::CNT_064_TIME_0064) 
                       == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    if (setButtonThresholds(defaultThresholds) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    
    // Currently disabling multiple touch detection and blocking
    if (setMultiTouchConfig(true, 1) == CAP1206_TRANSFER_FAIL) return CAP1206_TRANSFER_FAIL;
    
    return CAP1206_TRANSFER_SUCCESS;
}