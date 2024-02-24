// Lightly modified code for an RPi Pico spectrum analyzer http://radiopench.blog96.fc2.com/ 

#include <Arduino.h>
#include <Wire.h>
#include "arduinoFFT.h"
#include "is31fl3236.hpp"
#include "cap1206.hpp"
#include "led.hpp"

const pin_size_t R_IN = 26;
const pin_size_t L_IN = 27;

const uint16_t samples = 128;
const double samplingFrquency = 25641;
double vReal_R[samples];
double vImag_R[samples];
double vReal_L[samples];
double vImag_L[samples];
int16_t wave_R[samples];
int16_t wave_L[samples];

arduinoFFT FFTright = arduinoFFT(vReal_R, vImag_R, samples, samplingFrquency);
arduinoFFT FFTleft = arduinoFFT(vReal_L, vImag_L, samples, samplingFrquency);

TwoWire i2cBus(12, 13);

IS31FL3236 drivers[] = {
  IS31FL3236(0b01111000, 15, &i2cBus),
  IS31FL3236(0b01111110, 16, &i2cBus)
};

Cap1206 touch(&i2cBus);

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  delay(1000);

  initializeLED(drivers);

  drivers[0].initialize();
  drivers[1].initialize();
  
  touch.initialize();
}

void loop() {
  for (int i = 0; i < samples; i++) {
    wave_R[i] = analogRead(R_IN);
    wave_L[i] = analogRead(L_IN);
    delayMicroseconds(21); // Maintain 39us sampling period
  } // (5ms)

  // FFT Data preparation (1.7ms)
  for (int i = 0; i < samples; i++) {
    vReal_R[i] = (wave_R[i] - 2048) * 3.3 / 4096.0;
    vReal_L[i] = (wave_L[i] - 2048) * 3.3 / 4096.0;
    vImag_R[i] = 0;
    vImag_L[i] = 0;
  }
  // FFT Calculation  (33ms)
  FFTright.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFTleft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);

  FFTright.Compute(FFT_FORWARD);
  FFTleft.Compute(FFT_FORWARD);

  FFTright.ComplexToMagnitude();
  FFTleft.ComplexToMagnitude();
  // Magnitude is in `vReal_x` arrays

  delay(1);
}