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

#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define ADC_UNIT ADC_UNIT_1
#define ADC_BIT_WIDTH ADC_BITWIDTH_12

#define ADC_NUM_CHANNELS 5
#define ADC_MAX_CHANNELS 5

typedef struct samplingConf {
    int rate; // Raw samples per second
    int subSamples; // Four Channels * Four bytes // The number of sequential samples for each measurement
    adc_atten_t attenuation;
} samplingConf;

typedef struct channelsConf {
    int count;
    int ports[ADC_MAX_CHANNELS];
} channelsConf;

typedef struct AdcConfig {
    samplingConf sampling{};
    channelsConf channels{};
} AdcConfig;

class Adc {
public:

    Adc(AdcConfig conf);

    static void begin(void * params);

    void capture(int us);
    AdcConfig getConfig() {
        return conf;
    };

    Buffer *buffers[ADC_NUM_CHANNELS]{};

private:

    AdcConfig conf;

    int numSamples;
    int bufferSize;

    void setup(adc_continuous_handle_t *out_handle);

    adc_continuous_handle_t handle{};

    uint64_t cap = 0;

    bool running = false;
    bool idle = true;

    uint64_t cycles = 0;
    uint64_t duration = 0;
    double capPerSec = 0;


};


#endif //VOLUMETRIC_RADAR_ADC_H
