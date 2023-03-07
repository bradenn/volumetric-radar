//
// Created by Braden Nicholson on 2/6/23.
//

#ifndef VOLUMETRIC_RADAR_ADC_H
#define VOLUMETRIC_RADAR_ADC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "buffer.h"

#define ADC_FREQUENCY SOC_ADC_SAMPLE_FREQ_THRES_HIGH
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2

#define ADC_CONV_BUFFER (64*4*8)
#define ADC_CONV_FRAME (int) (256)

#define ADC_ATTENUATION ADC_ATTEN_DB_2_5
#define ADC_UNIT ADC_UNIT_1
#define ADC_BIT_WIDTH ADC_BITWIDTH_12

#define ADC_NUM_CHANNELS 5
#define ADC_MAX_CHANNELS 5

struct samplingConf {
    int rate = 80000; // Raw samples per second (including subSamples)
    int subSamples = 8; // The number of sequential samples for each measurement
    int bufferSize = (subSamples * 128); // The number of samples to record before overflowing the buffer
    adc_atten_t attenuation = ADC_ATTEN_DB_0;
};
struct channelsConf {
    int count = 4;
    int ports[ADC_MAX_CHANNELS] = {3, 4, 6, 5};
};

struct AdcConfig {
    samplingConf sampling{};
    channelsConf channels{};
};

class Adc {
public:

    Adc();

    Adc(AdcConfig *conf);

    static void begin(void * params);

    void capture(int us);

    void restart();

    void start();

    void stop();

    Buffer *buffers[ADC_NUM_CHANNELS]{};

    double getUpdatesPerSecond() {
        return capPerSec;
    }

private:

    AdcConfig *conf = nullptr;

    void setup(adc_continuous_handle_t *out_handle);

    adc_continuous_handle_t handle;

    uint64_t cap = 0;

    bool running = false;
    bool idle = true;

    uint64_t cycles = 0;
    uint64_t duration = 0;
    double capPerSec = 0;


};


#endif //VOLUMETRIC_RADAR_ADC_H
