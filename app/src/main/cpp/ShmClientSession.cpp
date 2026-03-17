//
// Created by Tom on 2026/3/12.
//

#include "ShmClientSession.h"
#include "ShmIpcMessage.h"


void ShmClientSession::startRunReadThreadLoop() {
    mShmReadThreadRunning = true;
    mShmProgressThread.reset(new std::thread(&ShmClientSession::messageProcessor, this));
    mShmReadThread.reset(new std::thread(&ShmClientSession::clientUdsReader, this));
}

void ShmClientSession::stopRunReadThreadLoop() {
    mShmReadThreadRunning = false;
    if (mShmReadThread && mShmReadThread->joinable()) {
        mShmReadThread->join();
    }
    mMessageQueue.stop();
    if (mShmProgressThread && mShmProgressThread->joinable()) {
        mShmProgressThread->join();
    }
}

void ShmClientSession::clientUdsReader() {

    LOGI("ShmClient Session Thread Start");

    uint8_t udsProtocolHeader[SHM_SERVER_PROTOCOL_HEAD_SIZE];
    std::vector<int> received_fds;
    int ret = -1;

    while (mShmReadThreadRunning) {
        ret = mShmProtocolHandler->receiveProtocolHeader(mClientFd, udsProtocolHeader, received_fds);
        if (ret) {
            ShmIpcMessage msg;
            msg.header = ShmIpcMessageHeader::deserialize(udsProtocolHeader);
            uint32_t payloadLength = msg.header.length - SHM_SERVER_PROTOCOL_HEAD_SIZE;
            std::vector<char> payload(payloadLength);
            if (payloadLength > 0) {
                if (!mShmProtocolHandler->receiveProtocolPayload(mClientFd, payload.data(), payloadLength)) {
                    LOGE("unix domain socket payload read failure");
                    break;
                }
            }
            msg.payload = std::move(payload);
            msg.fds = std::move(received_fds);
            mMessageQueue.push(std::move(msg));
        } else if (ret == 0) {
            LOGE("unix domain socket header read failure");
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            LOGE("unix domain socket header read failure");
            break;
        }
    }
    close(mClientFd);

    mShmReadThreadRunning = false;

    mMessageQueue.stop();
    if (mShmProgressThread && mShmProgressThread->joinable()) {
        mShmProgressThread->join();
    }

    LOGI("ShmClient Session Thread Stop");
}

void ShmClientSession::messageProcessor() {
    LOGI("Message Processor Thread Start");

    ShmIpcMessage msg;
    while (mMessageQueue.pop(msg)) {
        LOGI("Processing message, payload size: %zu, fds count: %zu",
             msg.payload.size(), msg.fds.size());
        auto messageType = static_cast<ShmProtocolType>(msg.header.type);
        switch (messageType) {

            case ShmProtocolType::ExchangeMetadata: {
                mShmProtocolHandler->exchangeMetaData(msg);
                break;
            }

            case ShmProtocolType::ShareMemoryByMemfd: {
                mShmProtocolHandler->shareMemoryByMemfd(msg);
                break;
            }

            default:
                break;
        }
    }

    LOGI("Message Processor Thread Stop");
}

void ShmClientSession::onSharedMemoryReady(void *addr, size_t size, int fd, ShmBufferManager *manager) {
    mSharedMemoryAddr = addr;
    mSharedMemorySize = size;
    mSharedMemoryFd = fd;
    mBufferManager = manager;

    LOGI("ShmClientSession received shared memory: addr=%p, size=%zu", addr, size);

//// 准备数据
//    const char data[] = "hello shm!";
//    writData(reinterpret_cast<const uint8_t*>(data), sizeof(data));

// 访问第一个 slice
    if (mBufferManager->buffer_list.slice_count > 0) {
        ShmBufferSlice* first_slice = mBufferManager->buffer_list.slices;
        first_slice->length = 16;
        first_slice->data[0] = 0xAA;
    }

}

void ShmClientSession::writData(const uint8_t *msg, uint32_t len) {
    if (!mBufferManager || !msg || len == 0) return;

    LOGD("writData len is : %d", len);

    auto* mgr = mBufferManager;
    auto* list = &mgr->buffer_list;
    auto* queue = &mgr->io_queue;

    uint32_t slices_needed = (len + SLICE_SIZE - 1) / SLICE_SIZE;
    uint32_t first_slice = INVALID_INDEX;
    uint32_t prev_slice = INVALID_INDEX;
    uint32_t offset = 0;

    for (uint32_t i = 0; i < slices_needed; ++i) {
        uint32_t idx = alloc_slice(list);
        if (idx == INVALID_INDEX) {
            uint32_t back = first_slice;
            while (back != INVALID_INDEX) {
                uint32_t next = list->slices[back].next;
                free_slice(list, back);
                back = next;
            }
            LOGE("No free slice available!");
            return;
        }

        auto& slice = list->slices[idx];
        uint32_t copy_len = std::min(len - offset, static_cast<uint32_t>(SLICE_SIZE));
        memcpy(slice.data, msg + offset, copy_len);
        slice.length = copy_len;
        offset += copy_len;

        if (prev_slice != INVALID_INDEX)
            list->slices[prev_slice].next = idx;
        else
            first_slice = idx;

        prev_slice = idx;
        slice.next = INVALID_INDEX;
    }

    uint32_t tail = queue->tail.load(std::memory_order_acquire);
    uint32_t next_tail = (tail + 1) % queue->capacity;
    if (next_tail == queue->head.load(std::memory_order_acquire)) {

        LOGE("EventQueue is full!");
        uint32_t back = first_slice;
        while (back != INVALID_INDEX) {
            uint32_t next = list->slices[back].next;
            free_slice(list, back);
            back = next;
        }
        return;
    }

    queue->events[tail].slice_index = first_slice;
    queue->events[tail].length = len;
    queue->tail.store(next_tail, std::memory_order_release);
}
