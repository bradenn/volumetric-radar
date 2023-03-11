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
    int *front = buffer.front();
    buffer.pop_front();
    if (front != nullptr) {
        free(front);
    }
}

int Buffer::numBuffers() {
    return buffer.size();
}

int *Buffer::frontBuffer() {

    if (buffer.empty()) {
        return nullptr;
    } else {
        return buffer.front();
    }
}

void Buffer::initBuffer() {

    current = (int *) heap_caps_malloc(BUFFER_SIZE * sizeof(int), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    index = 0;
}

Buffer::Buffer() {
    buffer = deque<int *>();
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

void Buffer::push(int value) {
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
