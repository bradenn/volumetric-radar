//
// Created by Braden Nicholson on 2/22/23.
//

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <mutex>
#include "indicator.h"

#define LEDC_FREQUENCY 2500
#define LEDC_SPEED LEDC_LOW_SPEED_MODE
#define LEDC_RESOLUTION ((2^13)-1)

esp_err_t configurePin(ledc_channel_t channel, gpio_num_t gpio) {
    esp_err_t err;

    ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_SPEED,
            .duty_resolution  = LEDC_TIMER_13_BIT,
            .timer_num        = LEDC_TIMER_0,
            .freq_hz          = LEDC_FREQUENCY,
            .clk_cfg          = LEDC_AUTO_CLK
    };

    err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        return err;
    }

    ledc_channel_config_t ledc_channel = {
            .gpio_num       = gpio,
            .speed_mode     = LEDC_SPEED,
            .channel        = channel,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
    };

    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;

}

esp_err_t Indicator::setup() {

    esp_err_t err;

    err = configurePin(LEDC_CHANNEL_0, (gpio_num_t) CONFIG_vRADAR_INDICATOR_S1);
    if (err != ESP_OK) {
        return err;
    }

    err = configurePin(LEDC_CHANNEL_1, (gpio_num_t) CONFIG_vRADAR_INDICATOR_S2);
    if (err != ESP_OK) {
        return err;
    }

    err = configurePin(LEDC_CHANNEL_2, (gpio_num_t) CONFIG_vRADAR_INDICATOR_S3);
    if (err != ESP_OK) {
        return err;
    }


    return ESP_OK;

}

void Indicator::setBrightness(int brightness) {

    this->brightness = brightness;

}

void Indicator::setIndicatorDuty(int indicator, int value) {

    switch (indicator) {
        case 0:
            ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL_0, value);
            ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL_0);
            break;
        case 1:
            ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL_1, value);
            ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL_1);
            break;
        case 2:
            ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL_2, value);
            ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL_2);
            break;
    }
}

void Indicator::setIndicator(IndicatorType it, bool state) {


    stateMask &= ~(1UL << it);
    switch (it) {
        case POWER:
            ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL_0, state ? brightness : 0);
            ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL_0);
            break;
        case LINK:
            ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL_1, state ? brightness : 0);
            ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL_1);
            break;
        case FAULT:
            ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL_2, state ? brightness : 0);
            ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL_2);
            break;
    }


}

Indicator &Indicator::instance() {
    static Indicator the_instance{};
    return the_instance;
}

void indicatorThread(void *params) {
    auto indicator = &Indicator::instance();
    int mask = indicator->mask();

    static int step = -1;
    const int numSteps = 100;
    const int stepDelay = 10;

    while (1) {

        mask = indicator->mask();

        step = (step + 1) % numSteps;

        double div = (M_PI * 2.0) / numSteps;

        int value = (int) (LEDC_RESOLUTION/2.0 + sin(div * step) * (double) LEDC_RESOLUTION/2.0);

//        printf("%d\n", value);

        if (mask & (1 << 0)) {
            indicator->setIndicatorDuty(0, value);
        } else if (mask & (1 << 1)) {
            indicator->setIndicatorDuty(1, value);
        } else if (mask & (1 << 2)) {
            indicator->setIndicatorDuty(2, value);
        }


        vTaskDelay(pdMS_TO_TICKS(stepDelay));

    }


}


Indicator::Indicator() {
    if (setup() != ESP_OK) {
        printf("Setup failed!\n");
        return;
    }
    setIndicator(POWER, true);
    setIndicator(LINK, true);
    setIndicator(FAULT, true);
    usleep(250*1000);
    setIndicator(POWER, false);
    setIndicator(LINK, false);
    setIndicator(FAULT, false);
    xTaskCreatePinnedToCore(indicatorThread, "indicatorTimer", 2048, nullptr, 1, nullptr, 1);

}

int Indicator::mask() {
    return stateMask;
}

void Indicator::setPulsing(int indicator, bool value) {
    if (value) {
        stateMask ^= 1UL << indicator;
    }else{
        stateMask &= ~(1UL << indicator);
    }
}
