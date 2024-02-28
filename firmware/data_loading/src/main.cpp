#include <Arduino.h>
#include <Wire.h>
#include "is31fl3236.hpp"
#include "cap1206.hpp"
#include "led.hpp"

TwoWire i2cBus(12, 13);

IS31FL3236 drivers[] = {
    IS31FL3236(0x3C, 15, &i2cBus),
    IS31FL3236(0x3F, 16, &i2cBus)
};

Cap1206 touch(&i2cBus);

void setup() {
    Serial.begin(112500);
    //while (!Serial) delay(10); // Wait for USB to open for debug messages
    
    Serial.println("STARTING DATA BOARD....");

    initializeLED(drivers);
    Serial.println("LED Driver settings selected");

    int test[2];
    test[0] = drivers[0].initialize();
    test[1] = drivers[1].initialize();
    if ((test[0] == IS31_TRANSFER_SUCCESS) && (test[1] == IS31_TRANSFER_SUCCESS)) {
        Serial.println("LED DRIVERS CONFIGURED SUCCESSFULLY");
    }
    else Serial.println("LED DRIVER SETUP ERROR");
    
    if (touch.initialize() == CAP1206_TRANSFER_SUCCESS) Serial.println("TOUCH SENSOR SUCCESSFULLY CONFIGURED");
    else Serial.println("TOUCH SENSOR ERROR");

    delay(2000);
}

void loop() {
    uint8_t buttons = 0;
    touch.readSensors(&buttons);
    LEDfsm(ledFSMstates::BREATH, buttons);

    remap(drivers);
    drivers[0].updateDuties();
    drivers[1].updateDuties();
    delay(10);
}