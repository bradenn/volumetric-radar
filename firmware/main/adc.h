//
// Created by Braden Nicholson on 2/6/23.
//

#ifndef VOLUMETRIC_RADAR_ADC_H
#define VOLUMETRIC_RADAR_ADC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"

#define ADC_FREQUENCY (20 * 1000)
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1

#define ADC_CONV_BUFFER 1024
#define ADC_CONV_FRAME 256

#define ADC_ATTENUATION ADC_ATTEN_DB_0
#define ADC_UNIT ADC_UNIT_1
#define ADC_BIT_WIDTH ADC_BITWIDTH_12

#define ADC_NUM_CHANNELS 2
static TaskHandle_t adcTask;

typedef void (*spectrumCallBack)(int m, int n, int **arr);

class Adc {
public:
    static Adc &instance();

    Adc(const Adc &) = default;

    Adc &operator=(const Adc &) = delete;

    void hook(spectrumCallBack cb) { func = cb; }

    void begin();

private:
    Adc();

    adc_continuous_handle_t handle{};

    spectrumCallBack func = nullptr;

    void setup();

    void overflow(int m, int n, int **res);
};


#endif //VOLUMETRIC_RADAR_ADC_H
