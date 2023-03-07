//
// Created by Braden Nicholson on 3/5/23.
//

#ifndef RADAR_CONTROLLER_H
#define RADAR_CONTROLLER_H


#include "iostream"
#include "string"
#include "adc.h"

using std::string;

class Controller {
public:
    Controller();

    string getParameters();
    Adc *adc = nullptr;

private:



};


#endif //RADAR_CONTROLLER_H
