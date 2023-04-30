//
// Created by Braden Nicholson on 4/20/23.
//

#ifndef RADAR_SAMPLE_H
#define RADAR_SAMPLE_H


#include <esp_adc/adc_continuous.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include "settings.h"

#define SAMPLE_CHANNEL_COUNT 4
#define SAMPLE_CONVERSIONS_PER_FRAME 32
#define SAMPLE_CONVERSION_FRAME_SIZE (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * SAMPLE_CHANNEL_COUNT *SAMPLE_CONVERSIONS_PER_FRAME)

class Sample {
public:
    Sample();


    ~Sample();

    esp_err_t listen(int64_t chirpDuration, uint16_t **mk);

    Sampling sampling{};


    esp_err_t reinitialize();

private:

    esp_timer_handle_t configTimer{};
    adc_continuous_handle_t adcContinuousHandle{};
    adc_cali_handle_t calHandle{};
    SemaphoreHandle_t runtime;
    esp_err_t initializeConfigurationTimer();

    esp_err_t initializeCalibrationProfile();

    esp_err_t initializeContinuousAdc();

    esp_err_t destructCalibrationProfile();

    esp_err_t destructContinuousAdc();

    esp_err_t changeAttenuation(int32_t atten);

};


#endif //RADAR_SAMPLE_H
