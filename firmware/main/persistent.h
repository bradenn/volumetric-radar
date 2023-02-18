//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_PERSISTENT_H
#define RADAR_PERSISTENT_H
#include <nvs_flash.h>


class Persistent {
public:

    static Persistent &instance();

    Persistent(const Persistent &) = default;


    Persistent &operator=(const Persistent &) = delete;



    void writeString(const char *key, const char *value) const;

    void readString(const char *key, char *dest) const;

private:
    nvs_handle_t handle = 0;
    Persistent();
};


#endif //RADAR_PERSISTENT_H
