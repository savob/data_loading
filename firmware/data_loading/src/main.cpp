#include <Arduino.h>
#include <Watchdog.h>
#include <Wire.h>

#include "audio.hpp"
#include "enumerators.h"
#include "is31fl3236.hpp"
#include "cap1206.hpp"
#include "led.hpp"

// Duration for watchdog timer, must be sufficient for entire setup (specified in milliseconds)
const u_int32_t WATCHDOG_TIMEOUT = 100; 

// Period of inactivity (ms) to trigger recalibration on capacitance sensor
const unsigned long TOUCH_RECALIBRATION_PERIOD  = 10000UL; 
const unsigned long TOUCH_CHECK_PERIOD          =    10UL;  // Minimum period to poll the touch sensor (ms)
// Touch check period should be at most half the cycle time set for the CAP1206 

const pin_size_t statusLED[] = {17, 18, 19}; // Status LEDs by index (last one is red)
const pin_size_t button[] = {20, 21}; // User buttons by index

TwoWire i2cBus(12, 13);

IS31FL3236 drivers[] = {
    IS31FL3236(0x3C, 15, &i2cBus),
    IS31FL3236(0x3F, 16, &i2cBus)
};

Cap1206 touch(&i2cBus);

// Variables for audio processing
double left[64], right[64], leftRMS, rightRMS;

void setup() {
    // Immediately start watchdog in the event there's any glitch
    mbed::Watchdog &watchdog = mbed::Watchdog::get_instance();
    watchdog.start(WATCHDOG_TIMEOUT);

    bool badSetup = false; // Tracks if there was any failed configuration

    // Start with LED and button initialization to show system is live before potentially waiting for Serial
    for (int i = 0; i < 2; i++) pinMode(button[i], INPUT_PULLUP);
    for (int i = 0; i < 3; i++) {
        pinMode(statusLED[i], OUTPUT);
        digitalWrite(statusLED[i], HIGH);
        delay(0.95 * WATCHDOG_TIMEOUT); // To not trigger watchdog
        watchdog.kick();
    }

    SerialUSB.begin(112500);
    // while (!SerialUSB) {
    //     delay(10); // Wait for USB to open for debug messages
    //     watchdog.kick(); // Don't trigger watchdog while waiting
    // }
    SerialUSB.println("\n\nSTARTING DATA BOARD....");

    if (setupAudio() == 0) SerialUSB.println("AUDIO INPUT CONFIGURED SUCCESSFULLY");
    else {
        SerialUSB.println("AUDIO INPUT CONFIGURE ERROR");
        badSetup = true;
    }

    initializeLED(drivers);
    for (int i = 0; i < 2; i++) {
        SerialUSB.print("LED DRIVER ");
        SerialUSB.print(i);
        if (drivers[i].initialize() == IS31_TRANSFER_SUCCESS) SerialUSB.println(" CONFIGURED SUCCESSFULLY");
        else {
            SerialUSB.println(" CONFIGURE ERROR");
            badSetup = true;
        }
    }
    
    bool touchSuccess = touch.initialize() == CAP1206_TRANSFER_SUCCESS;
    if (touchSuccess) SerialUSB.println("TOUCH SENSOR CONFIGURED SUCCESSFULLY");
    else {
        SerialUSB.println("TOUCH SENSOR CONFIGURE ERROR");
        badSetup = true;
    }

    // Reboot if any configuration failed
    if (badSetup == true) {
        SerialUSB.println("\nHOLDING FOR WATCHDOG REBOOT\n");
        while (true) {
            delay(WATCHDOG_TIMEOUT);
            // Trap system here until watchdog reboot kicks in
        }
    }

    SerialUSB.println("\nLAUNCHING!\n");
    for (int i = 0; i < 3; i++) digitalWrite(statusLED[i], LOW);

    watchdog.kick(); // Needed before main loop
}

void loop() {
    static AudioProcessing sampleAudio = AudioProcessing::NO_AUDIO;

    // Check pads
    uint8_t pads = 0; // Bit mask of pressed pads
    static unsigned long nextTouchPoll = 0; // Marks next touch sensor polling
    // Polling would not be needed if the alert/interrupt pin from the CAP1206 was connected to the RP2040

    if (millis() > nextTouchPoll) {
        nextTouchPoll = millis() + TOUCH_CHECK_PERIOD;

        static unsigned long nextTouchRecalibration = 0;
        touch.readSensors(&pads);

        // Check if the periodic recalibration is needed if not catching touches
        if (pads != 0) nextTouchRecalibration = millis() + TOUCH_RECALIBRATION_PERIOD;
        if (millis() > nextTouchRecalibration) {
            touch.setCalibrations(0x0F);
            nextTouchRecalibration = millis() + TOUCH_RECALIBRATION_PERIOD;
        }
    }

    // Audio sampling if needed
    readAudio(left, right, &leftRMS, &rightRMS, sampleAudio);

    // LED FSMs usually take about 40 to 160 us to execute, peak at about 250
    sampleAudio = LEDfsm(pads, left, right, leftRMS, rightRMS); //, ledFSMstates::AUD_UNI, true);

    // Updating entire PWM buffer takes about 1 ms per chip updated
    remapLED(drivers);
    drivers[0].updateDuties();
    drivers[1].updateDuties();

    // A little heartbeat
    if (((millis() / 500) % 2) == 1) digitalWrite(statusLED[0], HIGH);
    else digitalWrite(statusLED[0], LOW);

    mbed::Watchdog::get_instance().kick(); // Update WDT to avoid unnecessary reboots
}