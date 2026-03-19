#include "ShmServerSession.h"
#include "ShmIpcMessage.h"


void ShmServerSession::startRunReadThreadLoop() {
    mShmReadThreadRunning = true;
    mShmProgressThread.reset(new std::thread(&ShmServerSession::messageProcessor, this));
    mShmReadThread.reset(new std::thread(&ShmServerSession::clientUdsReader, this));
}

void ShmServerSession::stopRunReadThreadLoop() {
    mShmReadThreadRunning = false;
    if (mShmReadThread && mShmReadThread->joinable()) {
        mShmReadThread->join();
    }
    mMessageQueue.stop();
    if (mShmProgressThread && mShmProgressThread->joinable()) {
        mShmProgressThread->join();
    }
}

void ShmServerSession::clientUdsReader() {

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

void ShmServerSession::messageProcessor() {
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

void ShmServerSession::onSharedMemoryReady(void *addr, size_t size, int fd, ShmBufferManager *manager) {
    mSharedMemoryAddr = addr;
    mSharedMemorySize = size;
    mSharedMemoryFd = fd;
    mBufferManager = manager;

    LOGI("ShmClientSession received shared memory: addr=%p, size=%zu", addr, size);

    const char data[] = "hello shm!";
    writData(reinterpret_cast<const uint8_t*>(data), sizeof(data));
}

void ShmServerSession::writData(const uint8_t* msg, uint32_t len) {
    if (!mBufferManager || !msg || len == 0) return;

    auto* mgr = mBufferManager;
    auto* list = &mgr->buffer_list;
    auto* queue = &mgr->io_queue;

    uint32_t slice_size = SLICE_SIZE;
    uint32_t slices_needed = (len + slice_size - 1) / slice_size;

    uint32_t first = INVALID_INDEX;
    uint32_t prev = INVALID_INDEX;
    uint32_t offset = 0;

    for (uint32_t i = 0; i < slices_needed; ++i) {
        uint32_t idx = alloc_slice(list);
        if (idx == INVALID_INDEX) {
            LOGE("No free slice!");
            uint32_t cur = first;
            while (cur != INVALID_INDEX) {
                uint32_t next = list->slices[cur].next;
                free_slice(list, cur);
                cur = next;
            }
            return;
        }

        auto& s = list->slices[idx];

        uint32_t copy_len = std::min(len - offset, slice_size);
        memcpy(s.data, msg + offset, copy_len);
        s.length = copy_len;

        offset += copy_len;

        if (prev != INVALID_INDEX)
            list->slices[prev].next = idx;
        else
            first = idx;

        prev = idx;
        s.next = INVALID_INDEX;
    }

    uint32_t tail = queue->tail.load(std::memory_order_acquire);
    uint32_t next_tail = (tail + 1) % queue->capacity;

    if (next_tail == queue->head.load(std::memory_order_acquire)) {
        LOGE("Queue full!");

        uint32_t cur = first;
        while (cur != INVALID_INDEX) {
            uint32_t next = list->slices[cur].next;
            free_slice(list, cur);
            cur = next;
        }
        return;
    }

    queue->events[tail].slice_index = first;
    queue->events[tail].length = len;

    queue->tail.store(next_tail, std::memory_order_release);

    dataSync();
}

void ShmServerSession::dataSync() {
    if (mClientFd != -1) {
        ShmIpcMessage dataSyncMsg;
        auto type_byte = static_cast<uint8_t>(ShmProtocolType::SyncEvent);
        dataSyncMsg.header = ShmIpcMessageHeader(type_byte, SHM_SERVER_PROTOCOL_HEAD_SIZE , 0);
        mShmProtocolHandler->sendShmMessage(mClientFd, dataSyncMsg);
    }
}
