//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_PERSISTENT_H
#define RADAR_PERSISTENT_H


class Persistent {
public:
    Persistent();

    void writeString(char *key, char *value) const;

    char *readString(char *str) const;

private:
    nvs_handle_t handle{};

};


#endif //RADAR_PERSISTENT_H
