//
// Created by Braden Nicholson on 4/19/23.
//

#ifndef RADAR_GYRO_H
#define RADAR_GYRO_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "lsm6dsm_reg.h"

typedef struct GyroData {
    float roll;
    float pitch;
} GyroData;

class Gyro {

public:
    Gyro();

//    explicit Gyro(RingbufHandle_t pVoid);


    Gyro(RingbufHandle_t handle);

    static void lsm6dsm_task(void *pvParameters);
};


#endif //RADAR_GYRO_H
