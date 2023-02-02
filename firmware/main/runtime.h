//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_RUNTIME_H
#define RADAR_RUNTIME_H


#include "persistent.h"

class Runtime {

public:
    Runtime();
    ~Runtime();

private:

    Persistent *persistent;


};


#endif //RADAR_RUNTIME_H
