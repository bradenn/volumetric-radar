//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_PERSISTENT_H
#define RADAR_PERSISTENT_H

#include "string"

using std::string;

class Persistent {
public:
    Persistent();

    void writeString(const string &key, const string &value) const;

    string readString(const string &str) const;

private:
    nvs_handle_t handle{};

};


#endif //RADAR_PERSISTENT_H
