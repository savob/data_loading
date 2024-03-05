// Lightly modified code for an RPi Pico spectrum analyzer http://radiopench.blog96.fc2.com/blog-entry-1184.html
#include <Arduino.h>

#include "audio.hpp"
#include "arduinoFFT.h"

const pin_size_t R_IN = 26;
const pin_size_t L_IN = 27;

const uint16_t NUM_AUDIO_SAMPLES = 128; // Number of samples taken for audio FFT
const uint16_t NUM_SPECTRUM = NUM_AUDIO_SAMPLES / 2; // Number of entries in the audio spectrograph

const double SAMPLE_FREQ = 25641; // Results in almost 200 Hz wide buckets
const unsigned int SAMPLE_PER_US = 1000000.0 * (1.0 / SAMPLE_FREQ);

double vReal_R[NUM_AUDIO_SAMPLES];
double vImag_R[NUM_AUDIO_SAMPLES];
double vReal_L[NUM_AUDIO_SAMPLES];
double vImag_L[NUM_AUDIO_SAMPLES];
int16_t wave_R[NUM_AUDIO_SAMPLES];
int16_t wave_L[NUM_AUDIO_SAMPLES];

arduinoFFT FFTright = arduinoFFT(vReal_R, vImag_R, NUM_AUDIO_SAMPLES, SAMPLE_FREQ);
arduinoFFT FFTleft = arduinoFFT(vReal_L, vImag_L, NUM_AUDIO_SAMPLES, SAMPLE_FREQ);

/**
 * \brief Sets up audio sampling system
 * 
 * \return Returns status, error if non-zero
 */
int setupAudio() {
    analogReadResolution(12);
    pinMode(R_IN, INPUT);
    pinMode(L_IN, INPUT);

    return 0;
}

/**
 * \brief Reads the audio stream and generates notable measurements
 * 
 * \warning This is blocking for a notable duration (40ish milliseconds)
 * 
 * \param leftMag Location to record frequency magnitudes for left channel
 * \param rightMag Location to record frequency magnitudes for right channel
 * \param leftRMS Location to record RMS of left channel
 * \param rightRMS Location to record RMS of right channel
 * 
 * \note All values are normalized such that they go from 0 to 1
 */
void readAudio(double leftMag[], double rightMag[], double* leftRMS, double* rightRMS) {
    // Reset RMS
    *leftRMS = 0;
    *rightRMS = 0;

    // Sample collection
    // Since this is blocking for the 5ms needed, I've stuffed in other sample related math
    // The normailizng and RMS math takes about 2.30ms to complete on it's own
    unsigned long nextMarkUS = micros();
    for (int i = 0; i < NUM_AUDIO_SAMPLES; i++) {
        wave_R[i] = analogRead(R_IN);
        wave_L[i] = analogRead(L_IN);

        // Normalize ADC readings
        vReal_R[i] = ((double)wave_R[i] / 2048.0) - 1.0;
        vReal_L[i] = ((double)wave_L[i] / 2048.0) - 1.0;
        vImag_R[i] = 0;
        vImag_L[i] = 0;

        // Accumulate RMS
        *leftRMS = *leftRMS + (vReal_L[i] * vReal_L[i]);
        *rightRMS = *rightRMS + (vReal_R[i] * vReal_R[i]);
        while(micros() - nextMarkUS < SAMPLE_PER_US){
            // Wait for next mark
        }
        nextMarkUS += SAMPLE_PER_US;
    }

    // Finish RMS calculations
    *leftRMS = *leftRMS / (float)NUM_AUDIO_SAMPLES;
    *rightRMS = *rightRMS / (float)NUM_AUDIO_SAMPLES;
    *leftRMS = sqrt(*leftRMS);
    *rightRMS = sqrt(*rightRMS);

    // FFT Calculation (33ms)
    FFTright.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFTleft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);

    FFTright.Compute(FFT_FORWARD);
    FFTleft.Compute(FFT_FORWARD);

    FFTright.ComplexToMagnitude();
    FFTleft.ComplexToMagnitude();
    // Magnitude is in `vReal_x` arrays

    // printSampling(); // Comment out FFT calculations before this if active

    // Copy normalized values to different memory location (10ms)
    for (int i = 0; i < NUM_SPECTRUM; i++) {
        leftMag[i] = normalizeFreqMag(vReal_L[i]);
        rightMag[i] = normalizeFreqMag(vReal_R[i]);
    }
}

/**
 * \brief Converts power of a frequency to a normalized value for later processing
 * 
 * \param mag Magnitude for a frequency bucket
 * 
 * \return Normalized power for a frequency (0 to 1)
 * 
 * \note Uses logarithmic scaling (akin to dB)
 */
double normalizeFreqMag(double mag) {
    const double OFFSET = 1.5;
    const double SCALING = 1.0 / OFFSET;

    double fy;
    fy = SCALING * (log10(mag) + OFFSET);

    if (fy < 0) fy = 0;
    else if (fy > 1.0) fy = 1.0;

    return fy;
}

/**
 * \brief Prints a summary of all the data calculated for selected channel
 * 
 * \warning This takes a couple of seconds to complete
 * 
 * \param left Use left channel if true, otherwise right.
 * 
 * \note This does all the FFT calculations internals so it should be called right after `vReal` and `vImag` are prepared
 */
void printSampling(bool left) {
    if (left == true) {
        Serial.println("Left Data:");
        printVector(vReal_L, NUM_AUDIO_SAMPLES, SamplingScale::SCL_TIME);
        FFTleft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        Serial.println("Left Weighed data:");
        printVector(vReal_L, NUM_AUDIO_SAMPLES, SamplingScale::SCL_TIME);
        FFTleft.Compute(FFT_FORWARD);
        Serial.println("Left Computed Real values:");
        printVector(vReal_L, NUM_AUDIO_SAMPLES, SamplingScale::SCL_INDEX);
        Serial.println("Left Computed Imaginary values:");
        printVector(vImag_L, NUM_AUDIO_SAMPLES, SamplingScale::SCL_INDEX);
        FFTleft.ComplexToMagnitude();
        Serial.println("Left Computed magnitudes:");
        printVector(vReal_L, NUM_SPECTRUM, SamplingScale::SCL_FREQUENCY);
        double x = FFTleft.MajorPeak();
        Serial.println(x, 6); //Print out what frequency is the most dominant.
    }
    else {
        Serial.println("Right Data:");
        printVector(vReal_R, NUM_AUDIO_SAMPLES, SamplingScale::SCL_TIME);
        FFTleft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        Serial.println("drivers[1].initialize();Right Weighed data:");
        printVector(vReal_R, NUM_AUDIO_SAMPLES, SamplingScale::SCL_TIME);
        FFTleft.Compute(FFT_FORWARD);
        Serial.println("Right Computed Real values:");
        printVector(vReal_R, NUM_AUDIO_SAMPLES, SamplingScale::SCL_INDEX);
        Serial.println("Right Computed Imaginary values:");
        printVector(vImag_R, NUM_AUDIO_SAMPLES, SamplingScale::SCL_INDEX);
        FFTleft.ComplexToMagnitude();
        Serial.println("Right Computed magnitudes:");
        printVector(vReal_R, NUM_SPECTRUM, SamplingScale::SCL_FREQUENCY);
        double x = FFTright.MajorPeak();
        Serial.println(x, 6); //Print out what frequency is the most dominant.
    }
}

/**
 * \brief Prints a vector with an abscissa label
 * 
 * \warning This takes some time to execute
 * 
 * \param vData The vector to print
 * \param bufferSize Length of the vector to be printed
 * \param scaleType The type of scale for the vector (e.g. "frequency")
 */
void printVector(double *vData, uint16_t bufferSize, SamplingScale scaleType) {
    for (uint16_t i = 0; i < bufferSize; i++) {
        double abscissa = 0;
        /* Print abscissa value */
        switch (scaleType) {
        case SamplingScale::SCL_TIME:
            abscissa = ((i * 1.0) / SAMPLE_FREQ);
            break;
        case SamplingScale::SCL_FREQUENCY:
            abscissa = ((i * 1.0 * SAMPLE_FREQ) / NUM_AUDIO_SAMPLES);
            break;
        default: // Just use index
            abscissa = i;
            break;
        }
        if (scaleType == SamplingScale::SCL_TIME) Serial.print(abscissa, 6);
        else Serial.print(abscissa, 0);

        if (scaleType == SamplingScale::SCL_FREQUENCY) Serial.print("Hz");
        Serial.print("\t");
        
        Serial.println(vData[i], 4);
    }
    Serial.println();
}