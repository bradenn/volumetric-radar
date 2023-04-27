//
// Created by Braden Nicholson on 3/15/23.
//

#ifndef RADAR_SETTINGS_H
#define RADAR_SETTINGS_H

#include <cstdint>
#include <freertos/semphr.h>

typedef struct Sampling {
    int32_t frequency = 20000;
    int32_t samples = 1;
    int32_t prf = 32000;
    int32_t pulse = 32000;
} Sampling;

typedef struct System {
    int32_t chirp = 1;
    int32_t audible = 0;
    int32_t enabled = 1;
    int32_t gyro = 1;
} System;

class Settings {
public:
    static Settings &instance();

    Settings(const Settings &) = default;

    Settings &operator=(const Settings &) = delete;

    void fromJson(const char * json);
    void push() const;

    Sampling getSampling();
    System getSystem();


    void systemFromJson(const char *json);
private:
    Settings();

    SemaphoreHandle_t lock;
    Sampling sampling;
    System system;
    void pull();

};


#endif //RADAR_SETTINGS_H
