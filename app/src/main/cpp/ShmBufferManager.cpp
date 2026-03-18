//
// Created by Tom on 2026/3/16.
//


#include "ShmBufferManager.h"
#include "ShmLogger.h"

uint32_t alloc_slice(ShmBufferList* list) {
    uint32_t head = list->free_head.load(std::memory_order_acquire);
    while (head != INVALID_INDEX) {
        uint32_t next = list->slices[head].next;
        if (list->free_head.compare_exchange_weak(head, next,
                                                  std::memory_order_acq_rel)) {
            return head;
        }
    }
    return INVALID_INDEX;
}

void free_slice(ShmBufferList* list, uint32_t idx) {
    uint32_t head = list->free_head.load(std::memory_order_acquire);
    do {
        list->slices[idx].next = head;
    } while (!list->free_head.compare_exchange_weak(head, idx,
                                                    std::memory_order_acq_rel));
}

ShmBufferManager* init_shm_buffer_manager(void* addr, size_t total_size) {
    if (!addr || total_size < sizeof(ShmBufferManager)) return nullptr;
    memset(addr, 0, total_size);
    auto* mgr = (ShmBufferManager*)addr;

    mgr->io_queue.head.store(0);
    mgr->io_queue.tail.store(0);
    mgr->io_queue.capacity = EVENT_QUEUE_SIZE;

    size_t header_size = sizeof(ShmBufferManager);
    size_t slice_header_size = sizeof(uint32_t) + sizeof(uint32_t);
    size_t available_bytes = total_size - header_size;
    auto slice_count = static_cast<uint32_t>(available_bytes / (sizeof(ShmBufferSlice)));

    mgr->buffer_list.slice_count = slice_count;
    mgr->buffer_list.free_head.store(0);

    ShmBufferSlice* slice_ptr = mgr->buffer_list.slices;
    for (uint32_t i = 0; i < slice_count; ++i) {
        slice_ptr[i].next = (i + 1 < slice_count) ? i + 1 : INVALID_INDEX;
        slice_ptr[i].length = 0;
    }

    return mgr;
}