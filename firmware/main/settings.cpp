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

    int32_t audible = 0, gyro = 0, enabled = 0;

    p.readInt("audible", &audible, 0);
    p.readInt("gyro", &gyro, 1);
    p.readInt("enable", &enabled, 1);


    int32_t prf = 0, duration = 0, steps = 0, padding = 0, resolution = 0;
    p.readInt("prf", &prf, 12500);
    p.readInt("duration", &duration, 12500);
    p.readInt("steps", &steps, 125);
    p.readInt("padding", &padding, 0);
    p.readInt("resolution", &resolution, 125);

    int32_t frequency = 0, samples = 0, attenuation = 0;
    p.readInt("frequency", &frequency, 20480);
    p.readInt("samples", &samples, 1);
    p.readInt("attenuation", &attenuation, 0);


    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1)) != pdTRUE) {
        return;
    }

    system.sampling = {
            .frequency = frequency,
            .samples = samples,
            .attenuation = attenuation
    };

    system.chirp = {
            .prf = prf,
            .duration = duration,
            .steps = steps,
            .padding = padding,
            .resolution = resolution,
    };

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

    p.writeInt("prf", system.chirp.prf);
    p.writeInt("duration", system.chirp.duration);
    p.writeInt("steps", system.chirp.steps);
    p.writeInt("padding", system.chirp.padding);
    p.writeInt("resolution", system.chirp.resolution);

    p.writeInt("frequency", system.sampling.frequency);
    p.writeInt("samples", system.sampling.samples);
    p.writeInt("attenuation", system.sampling.attenuation);

    p.writeInt("audible", system.audible);
    p.writeInt("gyro", system.gyro);
    p.writeInt("enable", system.enabled);
    xSemaphoreGive(lock);
}

void Settings::fromJson(const char *json) {
    auto request = cJSON_Parse(json);

    int rate = cJSON_GetObjectItem(request, "frequency")->valueint;
    int samples = cJSON_GetObjectItem(request, "samples")->valueint;

    if (xSemaphoreTake(lock, pdMS_TO_TICKS(1)) != pdTRUE) {
        return;
    }
    sampling.frequency = rate;
    sampling.samples = samples;
    xSemaphoreGive(lock);
}

void Settings::systemFromJson(const char *json) {
    auto request = cJSON_Parse(json);

    cJSON *chirp = cJSON_GetObjectItem(request, "chirp");
    cJSON *sample = cJSON_GetObjectItem(request, "sampling");
    int audible = cJSON_GetObjectItem(request, "audible")->valueint;
    int gyro = cJSON_GetObjectItem(request, "gyro")->valueint;
    int enable = cJSON_GetObjectItem(request, "enable")->valueint;

    if (xSemaphoreTake(lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        cJSON_Delete(request);
        return;
    }

    system.chirp = {
            .prf = cJSON_GetObjectItem(chirp, "prf")->valueint,
            .duration = cJSON_GetObjectItem(chirp, "duration")->valueint,
            .steps = cJSON_GetObjectItem(chirp, "steps")->valueint,
            .padding = cJSON_GetObjectItem(chirp, "padding")->valueint,
            .resolution = cJSON_GetObjectItem(chirp, "resolution")->valueint,
    };

    system.sampling = {
            .frequency = cJSON_GetObjectItem(sample, "frequency")->valueint,
            .samples = cJSON_GetObjectItem(sample, "samples")->valueint,
            .attenuation =cJSON_GetObjectItem(sample, "attenuation")->valueint
    };

    system.audible = audible;
    system.gyro = gyro;
    system.enabled = enable;

    xSemaphoreGive(lock);

    cJSON_Delete(request);
}

Sampling Settings::getSampling() {
    return sampling;
}

System Settings::getSystem() {
    return system;
}
