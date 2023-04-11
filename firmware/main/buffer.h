//
// Created by Braden Nicholson on 2/10/23.
//

#ifndef RADAR_BUFFER_H
#define RADAR_BUFFER_H

#include <cstdint>
#include <deque>
#include <freertos/semphr.h>

#define BUFFER_SIZE 256
#define BUFFER_COUNT 16

using std::deque;

class Buffer {

public:

    Buffer();

    void nextBuffer();

    void pop();

    bool hasNext();

    int *front();

    void push(int value);

    int numBuffers();

private:

    int index = 0;
    deque<int *> buffer{};
    int *current = nullptr;
    SemaphoreHandle_t mutex;

    void initBuffer();


};


#endif //RADAR_BUFFER_H
