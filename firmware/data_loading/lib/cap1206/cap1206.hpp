#ifndef CAP1206_HEADER
#define CAP1206_HEADER

#include <Arduino.h>
#include <Wire.h>

extern const int CAP1206_TRANSFER_FAIL;
extern const int CAP1206_TRANSFER_SUCCESS;

/* Bare-bones library for the CAP1206 sensor
    Definitely needs some work to polish up and complete to cover all the 
    features offered by this chip. However only working on what is strictly
    needed for the DATA project at the present.

    Missing features:
    - Multitouch
    - Standby
    - Power Button
    - Reading back some values related to configuration/calibration
    - Interrupt stuff
*/
enum RegistersCap1206 : uint8_t {
    MAIN_CTRL           = 0x00,
    GEN_STATUS          = 0x02,
    SENSOR_INPUT        = 0x03,
    NOISE_FLAG          = 0x0A,
    DELTA_1             = 0x10,
    DELTA_2             = 0x11,
    DELTA_3             = 0x12,
    DELTA_4             = 0x13,
    DELTA_5             = 0x14,
    DELTA_6             = 0x15,
    SENS_CTRL           = 0x1F,
    CONFIG_1            = 0x20,
    SENS_IN_EN          = 0x21,
    SENS_IN_CONF_1      = 0x22,
    SENS_IN_CONF_2      = 0x23,
    AVE_SAMP_CONF       = 0x24,
    CALIB_ACT_STAT      = 0x26,
    INT_EN              = 0x27,
    REPEAT_RATE_EN      = 0x28,
    MUL_TOUCH_CONF      = 0x2A,
    MUL_TOUCH_PATT_CONF = 0x2B,
    MUL_TOUCH_PATT      = 0x2D,
    BASE_CNT_OOL        = 0x2E,
    RECAL_CONF          = 0x2F,
    SENS_THRS_1         = 0x30,
    SENS_THRS_2         = 0x31,
    SENS_THRS_3         = 0x32,
    SENS_THRS_4         = 0x33,
    SENS_THRS_5         = 0x34,
    SENS_THRS_6         = 0x35,
    SENS_NOISE_THRS     = 0x38,
    STBY_CHL            = 0x40,
    STBY_CONF           = 0x41,
    STBY_SENS           = 0x42,
    STBY_THRS           = 0x43,
    CONFIG_2            = 0x44,
    SENS_CNT_1          = 0x50,
    SENS_CNT_2          = 0x51,
    SENS_CNT_3          = 0x52,
    SENS_CNT_4          = 0x53,
    SENS_CNT_5          = 0x54,
    SENS_CNT_6          = 0x55,
    PWR_BUT             = 0x60,
    PWR_BUT_CONF        = 0x61,
    SENS_CALIB_1        = 0xB1,
    SENS_CALIB_2        = 0xB2,
    SENS_CALIB_3        = 0xB3,
    SENS_CALIB_4        = 0xB4,
    SENS_CALIB_5        = 0xB5,
    SENS_CALIB_6        = 0xB6,
    SENS_CALIB_LSB_1    = 0xB9,
    SENS_CALIB_LSB_2    = 0xBA,
    PROD_ID             = 0xFD,
    MANU_ID             = 0xFE,
    REV                 = 0xFF
};

enum DeltaSensitivityCap1206 : uint8_t {
    MUL_001             = 0x07,
    MUL_002             = 0x06,
    MUL_004             = 0x05,
    MUL_008             = 0x04,
    MUL_016             = 0x03,
    MUL_032             = 0x02,
    MUL_064             = 0x01,
    MUL_128             = 0x00,
};

enum BaseShiftCap1206 : uint8_t {
    SACLE_001           = 0x00,
    SACLE_002           = 0x01,
    SACLE_005           = 0x02,
    SACLE_008           = 0x03,
    SACLE_016           = 0x04,
    SACLE_032           = 0x05,
    SACLE_064           = 0x06,
    SACLE_128           = 0x07,
    SACLE_256           = 0x0F
};

enum MaxDurationcap1206 : uint8_t {
    MAX_DUR_00560       = 0x00,
    MAX_DUR_00840       = 0x01,
    MAX_DUR_01120       = 0x02,
    MAX_DUR_01400       = 0x03,
    MAX_DUR_01680       = 0x04,
    MAX_DUR_02240       = 0x05,
    MAX_DUR_02800       = 0x06,
    MAX_DUR_03360       = 0x07,
    MAX_DUR_03920       = 0x08,
    MAX_DUR_04480       = 0x09,
    MAX_DUR_05600       = 0x0A,
    MAX_DUR_06720       = 0x0B,
    MAX_DUR_07840       = 0x0C,
    MAX_DUR_08906       = 0x0D,
    MAX_DUR_10080       = 0x0E,
    MAX_DUR_11200       = 0x0F
};

enum RepeatRateCap1206 : uint8_t {
    REP_RATE_035              = 0x00,
    REP_RATE_070              = 0x01,
    REP_RATE_105              = 0x02,
    REP_RATE_140              = 0x03,
    REP_RATE_175              = 0x04,
    REP_RATE_210              = 0x05,
    REP_RATE_245              = 0x06,
    REP_RATE_280              = 0x07,
    REP_RATE_315              = 0x08,
    REP_RATE_350              = 0x09,
    REP_RATE_385              = 0x0A,
    REP_RATE_420              = 0x0B,
    REP_RATE_455              = 0x0C,
    REP_RATE_490              = 0x0D,
    REP_RATE_525              = 0x0E,
    REP_RATE_560              = 0x0F,
};

enum MinForRepeatCap1206 : uint8_t {
    MIN_PER_035              = 0x00,
    MIN_PER_070              = 0x01,
    MIN_PER_105              = 0x02,
    MIN_PER_140              = 0x03,
    MIN_PER_175              = 0x04,
    MIN_PER_210              = 0x05,
    MIN_PER_245              = 0x06,
    MIN_PER_280              = 0x07,
    MIN_PER_315              = 0x08,
    MIN_PER_350              = 0x09,
    MIN_PER_385              = 0x0A,
    MIN_PER_420              = 0x0B,
    MIN_PER_455              = 0x0C,
    MIN_PER_490              = 0x0D,
    MIN_PER_525              = 0x0E,
    MIN_PER_560              = 0x0F,
};

enum AveragedSamplesCap1206 : uint8_t {
    SMPL_001            = 0x00,
    SMPL_002            = 0x01,
    SMPL_004            = 0x02,
    SMPL_008            = 0x03,
    SMPL_016            = 0x04,
    SMPL_032            = 0x05,
    SMPL_064            = 0x06,
    SMPL_128            = 0x07,
};

enum SampleTimeCap1206 : uint8_t {
    US_0320             = 0x00,
    US_0640             = 0x01,
    US_1280             = 0x02,
    US_2560             = 0x03
};

enum CycleTime1206 : uint8_t {
    MS_035              = 0x00,
    MS_070              = 0x01,
    MS_105              = 0x02,
    MS_140              = 0x03
};

enum NegDeltaCountCap1206 : uint8_t {
    COUNT_08            = 0x00,
    COUNT_16            = 0x01,
    COUNT_32            = 0x02,
    NONE                = 0x03,
};

enum CalConfigCap1206 : uint8_t {
    CNT_016_TIME_0016   = 0x00,
    CNT_032_TIME_0032   = 0x01,
    CNT_064_TIME_0064   = 0x02,
    CNT_128_TIME_0128   = 0x03,
    CNT_256_TIME_0256   = 0x04,
    CNT_256_TIME_1024   = 0x05,
    CNT_256_TIME_2048   = 0x06,
    CNT_256_TIME_4096   = 0x07
};

enum SensNoiseThrsCap1206 : uint8_t {
    PER_250             = 0x00,
    PER_375             = 0x01,
    PER_500             = 0x02,
    PER_625             = 0x03
};

class Cap1206 {
private:
    const uint8_t ADDRESS_CAP = 0x28;
    TwoWire* interface;

    bool standbyEn = false;
    bool deepSleepEn = false;

    bool sensors[6] = {false};

    int writeSingleReg(RegistersCap1206 reg, uint8_t val);
    int readSingleReg(RegistersCap1206 reg, uint8_t* tar);
    int readManyRegs(RegistersCap1206 reg, uint8_t num, uint8_t* tar);
public:
    Cap1206(TwoWire* bus);

    int initialize();
    int setMainControl(bool stby, bool dslp, bool clrInt);
    int checkMainControl(uint8_t* tar);
    int checkGenStatus(uint8_t* tar);
    int checkGenStatusFlags(bool* bc, bool* acal, bool* pwr, bool*mult, bool*mtp, bool* touch);
    int checkInterrupt(bool* tar);
    int clearInterrupt();
    int readSensors(bool target[]);
    int readSensors(uint8_t* target);
    int readNoiseFlags(bool target[]);
    int readNoiseFlags(uint8_t* target);
    int setSensitivity(DeltaSensitivityCap1206 sens, BaseShiftCap1206 shift);
    int setConfig1(bool smbTO, bool disDigNoise, bool disAnaNoise, bool maxDurEn);
    int setConfig2(bool bcOutRecal, bool powReduction, bool bcOutInt, bool showRFnoiseOnly, bool disRFnoise, bool anaCalFailInt, bool intRelease);
    int enableSensors(bool sensors[]);
    int enableSensors(uint8_t sensors);
    int enableRepeat(bool sensors[]);
    int enableRepeat(uint8_t sensors);

    int setSensorInputConfig1(MaxDurationcap1206 dur, RepeatRateCap1206 rep);
    int setSensorInputConfig2(MinForRepeatCap1206 min);
    int setAverageAndSampling(AveragedSamplesCap1206 ave, SampleTimeCap1206 sam, CycleTime1206 cyc);
    int setSensorInputNoiseThreshold(SensNoiseThrsCap1206 thrs);
    int setCalibrations(bool sensors[]);
    int setCalibrations(uint8_t sensors);
    int readCalibrations(bool sensors[]);
    int readCalibrations(uint8_t* sensors);
    int enableInterrupt(bool sensors[]);
    int enableInterrupt(uint8_t sensors);
    int setRecalConfig(bool ldth, bool clrint, bool clrneg, NegDeltaCountCap1206 negcnt, CalConfigCap1206 cal);
    int setButtonThreshold(uint8_t but, uint8_t thres);
    int setButtonThresholds(uint8_t thres[]);
};

#endif