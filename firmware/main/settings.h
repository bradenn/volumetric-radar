//
// Created by Braden Nicholson on 3/15/23.
//

#ifndef RADAR_SETTINGS_H
#define RADAR_SETTINGS_H

#include <cstdint>

typedef struct Sampling {
    int32_t frequency = 40000;
    int32_t samples = 2;
    int32_t prf = 2000;
    int32_t pulse = 200;
} Sampling;

class Settings {
public:
    static Settings &instance();

    Settings(const Settings &) = default;


    Settings &operator=(const Settings &) = delete;

    void fromJson(const char * json);
    void push() const;
    Sampling sampling{};
private:
    Settings();





    void pull();
};


#endif //RADAR_SETTINGS_H
