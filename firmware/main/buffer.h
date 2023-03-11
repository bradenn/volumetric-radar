//
// Created by Braden Nicholson on 2/10/23.
//

#ifndef RADAR_BUFFER_H
#define RADAR_BUFFER_H

#include <cstdint>
#include <deque>

#define BUFFER_SIZE 480
#define BUFFER_COUNT 3

using std::deque;

class Buffer {

public:

    Buffer();

    void nextBuffer();

    void popBuffer();

    bool hasNext();

    int *frontBuffer();

    void push(int value);

    int numBuffers();

private:

    int index = 0;
    deque<int *> buffer{};
    int *current = nullptr;

    void initBuffer();


};


#endif //RADAR_BUFFER_H
