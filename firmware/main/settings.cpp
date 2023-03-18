//
// Created by Braden Nicholson on 3/15/23.
//

#include <cJSON.h>
#include "settings.h"
#include "persistent.h"

Settings &Settings::instance() {
    static Settings the_instance;
    return the_instance;
}

Settings::Settings() {
    pull();
}

void Settings::pull() {
    auto p = Persistent::instance();
    p.readInt("frequency", &this->sampling.frequency, 40000);
    p.readInt("samples", &this->sampling.samples, 2);
    p.readInt("prf", &this->sampling.prf, 1000);
    p.readInt("pulse", &this->sampling.pulse, 100);
}

void Settings::push() const {
    auto p = Persistent::instance();
    p.writeInt("frequency", this->sampling.frequency);
    p.writeInt("samples", this->sampling.samples);
    p.writeInt("prf", this->sampling.prf);
    p.writeInt("pulse", this->sampling.pulse);
}

void Settings::fromJson(const char *json) {
    auto request = cJSON_Parse(json);

    int rate = cJSON_GetObjectItem(request, "frequency")->valueint;
    int samples = cJSON_GetObjectItem(request, "samples")->valueint;
    int pulseRepetition = cJSON_GetObjectItem(request, "prf")->valueint;
    int pulse = cJSON_GetObjectItem(request, "pulse")->valueint;

    this->sampling.frequency = rate;
    this->sampling.samples = samples;
    this->sampling.prf = pulseRepetition;
    this->sampling.pulse = pulse;

}
