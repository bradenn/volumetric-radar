//
// Created by Braden Nicholson on 2/22/23.
//

#ifndef RADAR_IO_H
#define RADAR_IO_H

enum IndicatorType {
    POWER = 0,
    LINK,
    FAULT,
};

enum PulseMode {
    SIN = 0,
};

class Indicator {

public:

    static Indicator &instance();

    Indicator(const Indicator &) = default;

    Indicator &operator=(const Indicator &) = delete;

    void setBrightness(int brightness);

    void setIndicatorDuty(int indicator, int value);

    void setPulsing(int indicator, bool value);

    void setIndicator(IndicatorType it, bool state);

    int mask();

private:

    int brightness = 255;
    int stateMask = 0;
    Indicator();

    esp_err_t setup();


};


#endif //RADAR_IO_H
