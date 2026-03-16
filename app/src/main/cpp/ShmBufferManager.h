//
// Created by Tom on 2026/3/16.
//

#ifndef SHMIPCC_SHMBUFFERMANAGER_H
#define SHMIPCC_SHMBUFFERMANAGER_H

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <algorithm>

#define EVENT_QUEUE_SIZE 10
#define SLICE_SIZE 16384 // 16KB
#define INVALID_INDEX 0xFFFFFFFF

/**
 * @param slice_index: start read buffer slice index.
 * @param length: length of buffer transport.
 *
 * 8个字节。
 */
struct ShmBufferEvent {
    uint32_t slice_index;
    uint32_t length;
};

/**
 * ShmBufferEventQueue 8个字节 * EVENT_QUEUE_SIZE
 *
 * 80 + 4 + 4 + 4 = 92
 */
struct ShmBufferEventQueue {
    std::atomic<uint32_t> head; // 4 个字节
    std::atomic<uint32_t> tail;// 4 个字节
    uint32_t capacity; // 4 个字节
    ShmBufferEvent events[EVENT_QUEUE_SIZE];
};

struct ShmBufferSlice {
    uint32_t next;
    uint32_t length;
    uint8_t data[SLICE_SIZE];
};

/**
    @param uint32_t slice_count slice 总数
    @param free_head 空闲 slice 链表头
    @param BufferSlice 指向共享内存里的 slice 数组
 */
struct ShmBufferList {
    uint32_t slice_count;
    std::atomic<uint32_t> free_head;
    ShmBufferSlice* slices;
};

struct ShmBufferManager {
    ShmBufferEventQueue io_queue;
    ShmBufferList buffer_list;
};


ShmBufferManager* init_buffer_manager(size_t shm_size);

#endif //SHMIPCC_SHMBUFFERMANAGER_H
