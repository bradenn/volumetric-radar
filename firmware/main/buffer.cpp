#include <cstdio>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "buffer.h"

//
// Created by Braden Nicholson on 2/10/23.
//
void Buffer::popBuffer() {
    if (buffer.empty()) {
        return;
    }
    uint16_t *front = buffer.front();
    buffer.pop_front();
    if (front != nullptr) {
        free(front);
    }
}

int Buffer::numBuffers() {
    return buffer.size();
}

uint16_t *Buffer::frontBuffer() {

    if (buffer.empty()) {
        return nullptr;
    } else {
        return buffer.front();
    }
}

void Buffer::initBuffer() {

    current = (uint16_t *) malloc(BUFFER_SIZE * sizeof(uint16_t));
    index = 0;
}

Buffer::Buffer() {
    buffer = deque<uint16_t *>();
    initBuffer();
}

void Buffer::nextBuffer() {

    if (buffer.size() < BUFFER_COUNT) {
        buffer.push_back(current);
        initBuffer();
    } else {
        printf("buffer overflow %lu\n", esp_get_free_heap_size());

    }
    index = 0;
}

void Buffer::push(uint16_t value) {
    if (current == nullptr) return;
    current[index] = value;
    index = (index + 1) % BUFFER_SIZE;
    if (index == 0) {
        nextBuffer();
    }
}

bool Buffer::hasNext() {
    return !buffer.empty();
}
