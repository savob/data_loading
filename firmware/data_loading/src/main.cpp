#include <Arduino.h>
#include <Wire.h>

#include "audio.hpp"
#include "is31fl3236.hpp"
#include "cap1206.hpp"
#include "led.hpp"

const pin_size_t statusLED[] = {17, 18, 19}; // Status LEDs by index (last one is red)
const pin_size_t button[] = {20, 21}; // User buttons by index

TwoWire i2cBus(12, 13);

IS31FL3236 drivers[] = {
    IS31FL3236(0x3C, 15, &i2cBus),
    IS31FL3236(0x3F, 16, &i2cBus)
};

Cap1206 touch(&i2cBus);

void setup() {
    // Start with LED and button initialization to show system is live before potentially waiting for Serial
    for (int i = 0; i < 2; i++) pinMode(button[i], INPUT_PULLUP);
    for (int i = 0; i < 3; i++) {
        pinMode(statusLED[i], OUTPUT);
        digitalWrite(statusLED[i], HIGH);
        delay(250);
    }

    Serial.begin(112500);
    while (!Serial) delay(10); // Wait for USB to open for debug messages
    Serial.println("\n\nSTARTING DATA BOARD....");

    if (setupAudio() == 0) Serial.println("AUDIO INPUT CONFIGURED SUCCESSFULLY");
    else Serial.println("AUDIO INPUT CONFIGURE ERROR");

    initializeLED(drivers);
    int test[] = {IS31_TRANSFER_SUCCESS, IS31_TRANSFER_SUCCESS};
    test[0] = drivers[0].initialize();
    test[1] = drivers[1].initialize();
    if ((test[0] == IS31_TRANSFER_SUCCESS) && (test[1] == IS31_TRANSFER_SUCCESS)) {
        Serial.println("LED DRIVERS CONFIGURED SUCCESSFULLY");
    }
    else Serial.println("LED DRIVER CONFIGURE ERROR");
    
    bool touchSuccess = touch.initialize() == CAP1206_TRANSFER_SUCCESS;
    if (touchSuccess) Serial.println("TOUCH SENSOR CONFIGURED SUCCESSFULLY");
    else Serial.println("TOUCH SENSOR CONFIGURE ERROR");

    Serial.println("LAUNCHING!\n");
    // delay(2000);
    for (int i = 0; i < 3; i++) {
        digitalWrite(statusLED[i], LOW);
    }
}

void loop() {
    uint8_t pads = 0;
    touch.readSensors(&pads);
    LEDfsm(ledFSMstates::BUMPS, pads);

    // double left[64], right[64], leftRMS, rightRMS;
    // readAudio(left, right, &leftRMS, &rightRMS);

    // Currently just throw up the spectrum for the left channel on most of the LEDs
    // for (int i = 0; i < 64; i++) LEDlevel[i] = left[i] * 255.0;

    remapLED(drivers);
    drivers[0].updateDuties();
    drivers[1].updateDuties();

    // A little heartbeat
    if (((millis() / 500) % 2) == 1) digitalWrite(statusLED[0], HIGH);
    else digitalWrite(statusLED[0], LOW);
}