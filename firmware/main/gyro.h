//
// Created by Braden Nicholson on 4/19/23.
//

#ifndef RADAR_GYRO_H
#define RADAR_GYRO_H

#include <esp_timer.h>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "lsm6dsm_reg.h"
#include "driver/i2c.h"

static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len);

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);

typedef struct GyroData {
    float roll;
    float pitch;
} GyroData;

class Gyro {

public:

    explicit Gyro(RingbufHandle_t handle);

    ~Gyro();

private:
    stmdev_ctx_t device{};

    TaskHandle_t task{};

    esp_err_t initializeGyroDevice();

    RingbufHandle_t ringBuffer;

    esp_timer_handle_t configTimer{};

    esp_timer_handle_t taskTimer{};

    esp_err_t initializeConfigurationTimer();

    int32_t rate = 0;

    static void configTimerCallback(void *params);

    esp_err_t initializeTaskTimer();

    static void taskTimerCallback(void *params);
};


#endif //RADAR_GYRO_H
