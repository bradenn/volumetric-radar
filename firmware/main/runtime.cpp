//
// Created by Braden Nicholson on 2/1/23.
//

#include "runtime.h"

Runtime::Runtime() {
    // Initialize the non-volatile storage manager
    persistent = new Persistent();

}

Runtime::~Runtime() {

    delete persistent;

}
