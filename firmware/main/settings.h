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
    int32_t attenuation = 0;
} Sampling;


// PRF = delay between start of each chirp
// stepDuration = duration of each level
// steps = number of steps per chirp
// padding = number of steps to add on each side of the chirp, subtracting from the total number of steps
//

// PRF = 2ms
// duration = 1ms (1000us)
// steps = 100 (step duration = 10us)

typedef struct Chirp {
    // microseconds between the start of each sequential chirp
    int32_t prf = 12500;
    // microsecond duration of the chirp
    int32_t duration = 12500; // assert(duration <= prf)
    // dac changes per duration
    int32_t steps = 125; // assert(steps <= duration)
    // steps to subtract from pos and neg edge of the chirp
    int32_t padding = 0; // assert(padding*2 < steps)
    // bandwidth resolution over step range
    int32_t resolution = 125;
} Chirp;


typedef struct System {
//    int32_t chirp = 1;
    int32_t audible = 0;
    int32_t enabled = 1;
    int32_t gyro = 1;
    Chirp chirp{};
    Sampling sampling{};
} System;

class Settings {
public:
    static Settings &instance();

    Settings(const Settings &) = default;

    Settings &operator=(const Settings &) = delete;

    void fromJson(const char *json);

    void push() const;

    Sampling getSampling();

    System getSystem();


    void systemFromJson(const char *json);

private:
    Settings();

    SemaphoreHandle_t lock;
    Sampling sampling;
    System system{};

    void pull();

};


#endif //RADAR_SETTINGS_H
