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


enum RuntimeState {
    INITIALIZE = 0,
    SETUP = 1,
    CONNECTING = 2,
    RUNNING = 3,
};

class Runtime {

public:
    Runtime();

    ~Runtime();


private:

    RuntimeState state{};
    Persistent persistent{};


};


#endif //RADAR_RUNTIME_H
