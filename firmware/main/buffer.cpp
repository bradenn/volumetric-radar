//
// Created by Braden Nicholson on 2/10/23.
//

#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "buffer.h"

/**
  * @brief Remove the current front item reference from the buffer. This does not free the memory.
  */
void Buffer::pop() {
    if (!buffer.empty()) {
        buffer.pop_front();
    }
}

int *Buffer::front() {
    if (buffer.empty()) {
        return nullptr;
    } else {
        int *front = nullptr;
        front = buffer.front();
        return front;
    }
}

int Buffer::numBuffers() {
    return (int) buffer.size();
}

void Buffer::initBuffer() {
    int *next = (int *) malloc(BUFFER_SIZE * sizeof(int));
    if (next == nullptr) {
        printf("Failed to allocated memory for new buffer. Dumping front buffer.\n");
        int *fr = buffer.front();
        if (fr != nullptr) {
            buffer.pop_front();
            free(fr);

            next = (int *) malloc(BUFFER_SIZE * sizeof(int));
            if (next != nullptr) {
                current = next;
            }

        }
    } else {
        current = next;
    }
    index = 0;
}

Buffer::Buffer() {
    mutex = xSemaphoreCreateMutex();
    buffer = deque<int *>();
    initBuffer();

}

void Buffer::nextBuffer() {
    if (buffer.size() < BUFFER_COUNT) {
        buffer.push_back(current);
        initBuffer();
    }
    index = 0;
}

void Buffer::push(int value) {
    if (current == nullptr) {
        return;
    }
    current[index] = value;
    index = (index + 1) % BUFFER_SIZE;
    if (index == 0) {
        nextBuffer();
    }
}

bool Buffer::hasNext() {
    return !buffer.empty();
}
