//
// Created by Braden Nicholson on 4/11/23.
//

#ifndef RADAR_DAC_H
#define RADAR_DAC_H


#include <driver/spi_master.h>
#include <driver/gptimer_types.h>
#include <freertos/ringbuf.h>
#include "settings.h"

class DAC {
public:
    explicit DAC(TaskHandle_t handle);

    ~DAC();

    spi_device_handle_t device{};
    TaskHandle_t adcHandle{};

    int32_t audible = 0;
    int32_t delay = 100;
    int32_t stepResolution = 10;
    int32_t interval = 100;

    Chirp chirp{};

    esp_err_t changeChirpInterval(int32_t interval);

private:

    gptimer_handle_t chirpTimer{};
    esp_timer_handle_t configTimer{};

    int initializeConfigurationTimer();

    esp_err_t initializeSPI();

    esp_err_t initializeChirpTimer();


};


#endif //RADAR_DAC_H
