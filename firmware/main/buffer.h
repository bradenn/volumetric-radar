//
// Created by Braden Nicholson on 2/10/23.
//

#ifndef RADAR_BUFFER_H
#define RADAR_BUFFER_H

#include <cstdint>
#include <deque>

#define BUFFER_SIZE 256
#define BUFFER_COUNT 24

using std::deque;

class Buffer {

public:

    Buffer();

    void nextBuffer();

    void popBuffer();

    bool hasNext();

    uint16_t *frontBuffer();

    void push(uint16_t value);

    int numBuffers();

private:

    int index = 0;
    deque<uint16_t *> buffer{};
    uint16_t *current = nullptr;

    void initBuffer();


};


#endif //RADAR_BUFFER_H
