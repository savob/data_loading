#ifndef IS31FL3236_HEADER
#define IS31FL3236_HEADER

#include <Arduino.h>
#include <Wire.h>

extern const int IS31_TRANSFER_FAIL;
extern const int IS31_TRANSFER_SUCCESS;

enum FrequencyIS31FL3236 : uint8_t {
    KHz_3       = 0x00,
    KHz_22      = 0x01
};

enum CurrentSettingIS31FL3236 : uint8_t {
    FULL        = 0x00,
    HALF        = 0x01,
    THIRD       = 0x02,
    QUARTER     = 0x03
};

struct ChannelIS31FL3236 {
    bool state = true; // LED state (on or off)
    enum CurrentSettingIS31FL3236 currentLimit = CurrentSettingIS31FL3236::FULL; // Current limit setting for a channel
};

enum RegistersIS31FL3236 : uint8_t {
    SHUTDOWN    = 0x00,

    PWM_00      = 0x01,
    PWM_01      = 0x02,
    PWM_02      = 0x03,
    PWM_03      = 0x04,
    PWM_04      = 0x05,
    PWM_05      = 0x06,
    PWM_06      = 0x07,
    PWM_07      = 0x08,
    PWM_08      = 0x09,
    PWM_09      = 0x0A,
    PWM_10      = 0x0B,
    PWM_11      = 0x0C,
    PWM_12      = 0x0D,
    PWM_13      = 0x0E,
    PWM_14      = 0x0F,
    PWM_15      = 0x10,
    PWM_16      = 0x11,
    PWM_17      = 0x12,
    PWM_18      = 0x13,
    PWM_19      = 0x14,
    PWM_20      = 0x15,
    PWM_21      = 0x16,
    PWM_22      = 0x17,
    PWM_23      = 0x18,
    PWM_24      = 0x19,
    PWM_25      = 0x1A,
    PWM_26      = 0x1B,
    PWM_27      = 0x1C,
    PWM_28      = 0x1D,
    PWM_29      = 0x1E,
    PWM_30      = 0x1F,
    PWM_31      = 0x20,
    PWM_32      = 0x21,
    PWM_33      = 0x22,
    PWM_34      = 0x23,
    PWM_35      = 0x24,

    PWM_UPDATE  = 0x25,

    CTRL_00     = 0x26,
    CTRL_01     = 0x27,
    CTRL_02     = 0x28,
    CTRL_03     = 0x29,
    CTRL_04     = 0x2A,
    CTRL_05     = 0x2B,
    CTRL_06     = 0x2C,
    CTRL_07     = 0x2D,
    CTRL_08     = 0x2E,
    CTRL_09     = 0x2F,
    CTRL_10     = 0x30,
    CTRL_11     = 0x31,
    CTRL_12     = 0x32,
    CTRL_13     = 0x33,
    CTRL_14     = 0x34,
    CTRL_15     = 0x35,
    CTRL_16     = 0x36,
    CTRL_17     = 0x37,
    CTRL_18     = 0x38,
    CTRL_19     = 0x39,
    CTRL_20     = 0x3A,
    CTRL_21     = 0x3B,
    CTRL_22     = 0x3C,
    CTRL_23     = 0x3D,
    CTRL_24     = 0x3E,
    CTRL_25     = 0x3F,
    CTRL_26     = 0x40,
    CTRL_27     = 0x41,
    CTRL_28     = 0x42,
    CTRL_29     = 0x43,
    CTRL_30     = 0x44,
    CTRL_31     = 0x45,
    CTRL_32     = 0x46,
    CTRL_33     = 0x47,
    CTRL_34     = 0x48,
    CTRL_35     = 0x49,

    CTRL_GLOBAL = 0x4A,
    FREQUENCY   = 0x4B,
    RESET       = 0x4F
};


class IS31FL3236 {
private: 
    const uint8_t ADDRESS;
    const pin_size_t SHUTDOWN_PIN;
    
    uint_fast8_t prevDuties[36] = {0}; // Records previous duties uploaded to LED drivers

    TwoWire* interface;

    int writeSingleRegister(RegistersIS31FL3236 reg, uint8_t val);

public:
    IS31FL3236(uint8_t add, pin_size_t shtdn, TwoWire* bus);

    struct ChannelIS31FL3236 channelConfig[36];
    uint8_t duty[36]; // PWM duty for each channel

    int initialize();
    void hardwareShutdown(bool shutdown);
    int softwareShutdown(bool shutdown);
    int globalEnable(bool en);
    int softwareReset();

    int setPWMfrequency(FrequencyIS31FL3236 freq);

    int updateChannelConfigurations();
    int updateDuties(bool forceUpdate = false);
};

#endif