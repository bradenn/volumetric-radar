//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_RUNTIME_H
#define RADAR_RUNTIME_H

#include <cstring>
#include <esp_netif.h>
#include <esp_event.h>
#include "adc.h"
#include "network.h"
#include "persistent.h"
#include "indicator.h"


enum RuntimeState {
    INITIALIZE = 0,
    SETUP = 1,
    CONNECTING = 2,
    RUNNING = 3,
};
typedef struct SampleData {
    uint16_t **data;
    int size;
    int64_t start;
    int64_t stop;
    int64_t chirpStart;
    int64_t chirpStop;
} SampleData;
class Runtime {

public:
    Runtime();

    ~Runtime();


private:

    RuntimeState state{};

    Indicator *indicator;


};


#endif //RADAR_RUNTIME_H
