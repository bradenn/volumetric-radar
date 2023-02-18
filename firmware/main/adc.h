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

#define ADC_FREQUENCY (128000)
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1

#define ADC_CONV_BUFFER 4000
#define ADC_CONV_FRAME 32

#define ADC_ATTENUATION ADC_ATTEN_DB_0
#define ADC_UNIT ADC_UNIT_1
#define ADC_BIT_WIDTH ADC_BITWIDTH_12

#define ADC_NUM_CHANNELS 1

class Adc {
public:
    Adc();

    void begin();

    Buffer *buffers[ADC_NUM_CHANNELS]{};

private:

    adc_continuous_handle_t handle;

    static void setup(adc_continuous_handle_t *out_handle);

};


#endif //VOLUMETRIC_RADAR_ADC_H
