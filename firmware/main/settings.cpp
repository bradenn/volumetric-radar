//
// Created by Braden Nicholson on 3/15/23.
//

#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include "settings.h"
#include "persistent.h"

Settings &Settings::instance() {
    static Settings the_instance = Settings();
    return the_instance;
}

Settings::Settings() {
    lock = xSemaphoreCreateMutex();
    pull();
}

void Settings::pull() {

    auto p = Persistent::instance();

    int32_t frequency = 0, samples = 0, prf = 0, pulse = 0;
    p.readInt("frequency", &frequency, 20000);
    p.readInt("samples", &samples, 1);
    p.readInt("prf", &prf, 32000);
    p.readInt("pulse", &pulse, 32000);

    int32_t chirp = 0, audible = 0, gyro = 0, enabled = 0;
    p.readInt("chirp", &chirp, 1);
    p.readInt("audible", &audible, 0);
    p.readInt("gyro", &gyro, 1);
    p.readInt("enable", &enabled, 1);


    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1)) != pdTRUE) {
        return;
    }

    sampling.frequency = frequency;
    sampling.samples = samples;
    sampling.prf = prf;
    sampling.pulse = pulse;

    system.chirp = chirp;
    system.audible = audible;
    system.enabled = enabled;
    system.gyro = gyro;

    xSemaphoreGive(lock);
}

void Settings::push() const {
    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1)) != pdTRUE) {
        printf("Could not push\n");
        return;
    }
    auto p = Persistent::instance();
    p.writeInt("frequency", sampling.frequency);
    p.writeInt("samples", sampling.samples);
    p.writeInt("prf", sampling.prf);
    p.writeInt("pulse", sampling.pulse);

    p.writeInt("chirp", system.chirp);
    p.writeInt("audible", system.audible);
    p.writeInt("gyro", system.gyro);
    p.writeInt("enable", system.enabled);
    xSemaphoreGive(lock);
}

void Settings::fromJson(const char *json) {
    auto request = cJSON_Parse(json);

    int rate = cJSON_GetObjectItem(request, "frequency")->valueint;
    int samples = cJSON_GetObjectItem(request, "samples")->valueint;
    int pulseRepetition = cJSON_GetObjectItem(request, "prf")->valueint;
    int pulse = cJSON_GetObjectItem(request, "pulse")->valueint;

    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1)) != pdTRUE) {
        return;
    }
    sampling.frequency = rate;
    sampling.samples = samples;
    sampling.prf = pulseRepetition;
    sampling.pulse = pulse;
    xSemaphoreGive(lock);
}

void Settings::systemFromJson(const char *json) {
    auto request = cJSON_Parse(json);

    int chirp = cJSON_GetObjectItem(request, "chirp")->valueint;
    int audible = cJSON_GetObjectItem(request, "audible")->valueint;
    int gyro = cJSON_GetObjectItem(request, "gyro")->valueint;
    int enable = cJSON_GetObjectItem(request, "enable")->valueint;

    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1)) != pdTRUE) {
        return;
    }
    system.chirp = chirp;
    system.audible = audible;
    system.gyro = gyro;
    system.enabled = enable;
    xSemaphoreGive(lock);
}

Sampling Settings::getSampling() {
    return sampling;
}

System Settings::getSystem() {
    return system;
}
