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


    Controller(Adc *adc, uint32_t prf, uint32_t pulse);


private:


    Controller(Adc *adc);
};


#endif //RADAR_CONTROLLER_H
