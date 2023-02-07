//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_PERSISTENT_H
#define RADAR_PERSISTENT_H
#include <nvs_flash.h>


class Persistent {
public:
    Persistent();

    void writeString(char *key, char *value) const;

    char * readString(char *str) const;

private:
    nvs_handle_t handle = 0;

};


#endif //RADAR_PERSISTENT_H
