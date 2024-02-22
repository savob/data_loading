#include <Arduino.h>

#include "is31fl3236.hpp"

const int IS31_TRANSFER_FAIL = -1;
const int IS31_TRANSFER_SUCCESS = 0;

const byte PWM_GAMMA_64[64] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0b,0x0d,0x0f,0x11,0x13,0x16,
  0x1a,0x1c,0x1d,0x1f,0x22,0x25,0x28,0x2e,
  0x34,0x38,0x3c,0x40,0x44,0x48,0x4b,0x4f,
  0x55,0x5a,0x5f,0x64,0x69,0x6d,0x72,0x77,
  0x7d,0x80,0x88,0x8d,0x94,0x9a,0xa0,0xa7,
  0xac,0xb0,0xb9,0xbf,0xc6,0xcb,0xcf,0xd6,
  0xe1,0xe9,0xed,0xf1,0xf6,0xfa,0xfe,0xff
};

/**
 * @brief Sets a single register's value on the IS31FL3236
 * 
 * @param reg Register address to write to
 * @param val New value to write to the register
 * @return Return status of the transfer 
 */
int IS31FL3236::writeSingleRegister(RegistersIS31FL3236 reg, uint8_t val) {
    uint8_t count = 0;

    interface->beginTransmission(ADDRESS); 
    count = interface->write(reg);
    count = count + interface->write(val);
    interface->endTransmission();

    // Check if both fields were communicated correctly
    if (count == 2) return IS31_TRANSFER_SUCCESS;
    return IS31_TRANSFER_FAIL;
}

/**
 * @brief Sets the software shutdown of an IS31FL3236
 * 
 * @param shutdown To put the chip in a software shutdown or not
 * 
 * @note IS31FL3236s default to a software shutdown state on power up
 */
int IS31FL3236::softwareShutdown(bool shutdown) {
    if (shutdown == false) return writeSingleRegister(RegistersIS31FL3236::SHUTDOWN, 0x01);
    else return writeSingleRegister(RegistersIS31FL3236::SHUTDOWN, 0x00);
}

/**
 * @brief Shutdown a given IS31FL3236 via its hardware shutdown pin
 * 
 * @param shutdown To shutdown chip or not through hardware pin
 */
void IS31FL3236::hardwareShutdown(bool shutdown) {
    digitalWrite(SHUTDOWN_PIN, !shutdown); // Shutdown is active low
}

/**
 * @brief Triggers a software reset of the IS31FL3236
 * 
 * @note All registers will return to defaults afterwards
 */
int IS31FL3236::softwareReset() {
    return writeSingleRegister(RegistersIS31FL3236::RESET, 0x00);
}

/**
 * @brief Updates the IS31FL3236 PWM frequency
 * 
 * @param freq New frequency setting to use
 * @return Return status of the transfer 
 */
int IS31FL3236::setPWMfrequency(FrequencyIS31FL3236 freq) {
    return writeSingleRegister(RegistersIS31FL3236::FREQUENCY, freq);
}

/**
 * @brief Globally enable/disable LED operation
 * 
 * @note No real idea how this is functionally different to a software shutdown
 * 
 * @param en Whether to enable normal LED operation or disable LEDs
 * @return Return status of the transfer 
 */
int IS31FL3236::globalEnable(bool en) {
    if (en) return writeSingleRegister(RegistersIS31FL3236::CTRL_GLOBAL, 0x00);
    else return writeSingleRegister(RegistersIS31FL3236::CTRL_GLOBAL, 0x01);
}

/**
 * @brief Updates the channel settings for all LED channels
 * 
 * @note This just updates channel configurations, it will not update the PWM duty. `updateDuties` is for that.
 * @return Return status of the transfer 
 */
int IS31FL3236::updateChannelConfigurations() {
    uint_fast8_t count = 0;

    interface->beginTransmission(ADDRESS); 

    // Start at channel 0 and use sequential write
    count = interface->write(RegistersIS31FL3236::CTRL_00);

    for (uint_fast8_t i = 0; i < 36; i++) {
        uint8_t temp = (channelConfig[i].currentLimit << 1) + channelConfig[i].state;
        count = count + interface->write(temp);
    }
    interface->endTransmission();

    // Check if all fields were communicated correctly (36 channels and the starting address)
    if (count != 37) return IS31_TRANSFER_FAIL;

    // Then write to move the values into the hardware
    return writeSingleRegister(RegistersIS31FL3236::PWM_UPDATE, 0x00);
}

/**
 * @brief Updates the PWM duty for each channel
 * 
 * @note This only updates the PWM duties, it will not update channel configuration. `updateChannelConfigurations` is for that.
 * @return Return status of the transfer 
 */
int IS31FL3236::updateDuties() {
    uint_fast8_t count = 0;

    interface->beginTransmission(ADDRESS); 

    // Start at channel 0 and use sequential write
    count = interface->write(RegistersIS31FL3236::PWM_00);

    for (uint_fast8_t i = 0; i < 36; i++) {
        count = count + interface->write(duty[i]);
    }
    count = count + interface->write(0x00); // Write to the PWM update register to have values reflected in hardware
    interface->endTransmission();

    // Check if all fields were communicated correctly (36 channels and the starting address)
    if (count == 38) return IS31_TRANSFER_SUCCESS;

    return IS31_TRANSFER_FAIL;
}

/**
 * @brief Starts the IS31FL3236 chip
 * 
 * @return int 
 */
int IS31FL3236::initialize() {

    pinMode(SHUTDOWN_PIN, OUTPUT);
    hardwareShutdown(false);

    interface->begin(); // Begin I2C bus

    if (softwareShutdown(false) == IS31_TRANSFER_FAIL) return IS31_TRANSFER_FAIL;
    if (setPWMfrequency(FrequencyIS31FL3236::KHz_22) == IS31_TRANSFER_FAIL) return IS31_TRANSFER_FAIL;
    if (globalEnable(true) == IS31_TRANSFER_FAIL) return IS31_TRANSFER_FAIL;

    for (uint_fast8_t i = 0; i < 36; i++) {
        channelConfig[i].state = true;
        channelConfig[i].currentLimit = CurrentSettingIS31FL3236::FULL;
    }
    if (updateChannelConfigurations() == IS31_TRANSFER_FAIL) return IS31_TRANSFER_FAIL;

    return IS31_TRANSFER_SUCCESS;
}

/**
 * @brief Construct a new IS31FL3236::IS31FL3236 object
 * 
 * @param add I2C address
 * @param shtdn Shutdown pin address
 * @param bus Pointer to I2C bus for the drivers
 */
IS31FL3236::IS31FL3236(uint8_t add, pin_size_t shtdn, TwoWire * bus) 
                        :ADDRESS(add), SHUTDOWN_PIN(shtdn) {
    interface = bus;
}

/**
 * @brief Sets the duty of an LED to gamma level specified
 * 
 * @note This accounts for the non-linear perception of light intensity by the eye
 * 
 * @param led LED index to set (will be constrained between 0 and 35)
 * @param gamma Gamma setting to use (range goes from 0 to 63 inclusive)
 */
void IS31FL3236::setGammaPWM(uint_fast8_t led, uint_fast8_t gamma) {
    if (led > 35) led = 0;
    if (gamma > 63) gamma = 0;
    duty[led] = PWM_GAMMA_64[gamma];
}