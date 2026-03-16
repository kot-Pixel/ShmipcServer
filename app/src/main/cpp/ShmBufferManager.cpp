//
// Created by Tom on 2026/3/16.
//


#include "ShmBufferManager.h"
#include "ShmLogger.h"

// -------------------- slice 分配与释放 --------------------
uint32_t alloc_slice(ShmBufferList* list) {
    uint32_t index = list->free_head.load(std::memory_order_acquire);
    if (index == INVALID_INDEX) return INVALID_INDEX;

    ShmBufferSlice& slice = list->slices[index];
    uint32_t next = slice.next;
    list->free_head.store(next, std::memory_order_release);

    slice.length = 0;
    slice.next = INVALID_INDEX;
    return index;
}

void free_slice(ShmBufferList* list, uint32_t index) {
    ShmBufferSlice& slice = list->slices[index];
    slice.next = list->free_head.load(std::memory_order_acquire);
    list->free_head.store(index, std::memory_order_release);
}

bool write_message(ShmBufferManager* mgr, const uint8_t* msg, uint32_t len) {
    ShmBufferList* list = &mgr->buffer_list;
    ShmBufferEventQueue* queue = &mgr->io_queue;

    uint32_t slices_needed = (len + SLICE_SIZE - 1) / SLICE_SIZE;
    uint32_t first_slice = INVALID_INDEX;
    uint32_t prev_slice = INVALID_INDEX;
    uint32_t offset = 0;

    for (uint32_t i = 0; i < slices_needed; ++i) {
        uint32_t idx = alloc_slice(list);
        if (idx == INVALID_INDEX) return false;

        ShmBufferSlice& slice = list->slices[idx];
        uint32_t copy_len = (len - offset < SLICE_SIZE) ? (len - offset) : SLICE_SIZE;
        memcpy(slice.data, msg + offset, copy_len);
        slice.length = copy_len;
        offset += copy_len;

        if (prev_slice != INVALID_INDEX) list->slices[prev_slice].next = idx;
        else first_slice = idx;

        prev_slice = idx;
    }

    uint32_t tail = queue->tail.load(std::memory_order_acquire);
    queue->events[tail].slice_index = first_slice;
    queue->events[tail].length = len;
    queue->tail.store((tail + 1) % queue->capacity, std::memory_order_release);
    return true;
}

// -------------------- Client 读消息 --------------------
bool read_message(ShmBufferManager* mgr, std::vector<uint8_t>& out) {
    ShmBufferEventQueue* queue = &mgr->io_queue;
    ShmBufferList* list = &mgr->buffer_list;

    uint32_t head = queue->head.load(std::memory_order_acquire);
    if (head == queue->tail.load(std::memory_order_acquire)) return false;

    ShmBufferEvent& e = queue->events[head];
    out.clear();

    uint32_t idx = e.slice_index;
    uint32_t remaining = e.length;
    while (idx != INVALID_INDEX && remaining > 0) {
        ShmBufferSlice& slice = list->slices[idx];
        uint32_t len = std::min(remaining, slice.length);
        out.insert(out.end(), slice.data, slice.data + len);
        remaining -= len;

        uint32_t next = slice.next;
        free_slice(list, idx);
        idx = next;
    }

    queue->head.store((head + 1) % queue->capacity, std::memory_order_release);
    return true;
}

ShmBufferManager* init_buffer_manager(size_t shm_size) {

    LOGD("all map memory size is : %d Bytes", shm_size);

    auto* mgr = new ShmBufferManager();

    mgr->io_queue.head.store(0);
    mgr->io_queue.tail.store(0);
    mgr->io_queue.capacity = EVENT_QUEUE_SIZE;

    LOGD("io message queue size is: %d Bytes", sizeof(ShmBufferEventQueue));
    LOGD("shm buffer slice size is: %d Bytes", sizeof(ShmBufferSlice));

    size_t slice_space = shm_size - sizeof(ShmBufferEventQueue);
    auto slice_count = static_cast<uint32_t>(slice_space / sizeof(ShmBufferSlice));

    LOGD("all slice count is : %d", slice_count);

    if (slice_count == 0) return nullptr;

    mgr->buffer_list.slice_count = slice_count;
    mgr->buffer_list.slices = new ShmBufferSlice[slice_count];

    for (uint32_t i = 0; i < slice_count - 1; ++i) {
        mgr->buffer_list.slices[i].next = i + 1;
        mgr->buffer_list.slices[i].length = 0;
    }
    mgr->buffer_list.slices[slice_count - 1].next = INVALID_INDEX;
    mgr->buffer_list.slices[slice_count - 1].length = 0;

    mgr->buffer_list.free_head.store(0);

    return mgr;
}