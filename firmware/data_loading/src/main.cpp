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

//Cap1206 touch(&i2cBus);

void setup() {
  Serial.begin(9600);
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
  
  //touch.initialize();

  delay(2000);
}

void loop() {
  LEDfsm(ledFSMstates::BREATH, 0);
  remap(drivers);
  drivers[0].updateDuties();
  drivers[1].updateDuties();
  delay(10);
}