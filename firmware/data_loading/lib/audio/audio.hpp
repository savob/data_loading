#ifndef AUDIO_HEADER
#define AUDIO_HEADER

#include <Arduino.h>

int setupAudio();
void readAudio(double leftMag[], double rightMag[], double* leftRMS, double* rightRMS);
double normalizeFreqMag(double mag);

enum SamplingScale {
    SCL_INDEX       = 0x00,
    SCL_TIME        = 0x01,
    SCL_FREQUENCY   = 0x02,
    SCL_PLOT        = 0x03
};

void printSampling(bool left = true);
void printVector(double *vData, uint16_t bufferSize, SamplingScale scaleType);

#endif