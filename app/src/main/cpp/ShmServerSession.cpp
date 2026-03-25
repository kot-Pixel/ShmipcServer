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
//        LOGI("Processing message, payload size: %zu, fds count: %zu",
//             msg.payload.size(), msg.fds.size());
        auto messageType = static_cast<ShmProtocolType>(msg.header.type);
        switch (messageType) {

            case ShmProtocolType::ExchangeMetadata: {
                mShmProtocolHandler->exchangeMetaData(msg);
                break;
            }

            case ShmProtocolType::ShareMemoryByMemfd: {
                mShmProtocolHandler->handleShareMemoryByMemfd(msg);
                break;
            }

            case ShmProtocolType::AckShareMemory: {
                mShmProtocolHandler->shareMemoryByMemfd(msg);
                break;
            }

            case ShmProtocolType::SyncEvent: {
                mShmProtocolHandler->syncEvent(msg);
            }

            default:
                break;
        }
    }

    LOGI("Message Processor Thread Stop");
}

void ShmServerSession::onSharedMemoryReady(void *addr, size_t size, int fd, ShmBufferManager *manager) {
    LOGI(">>>>>>>>>> receive AckShareMemory");
    mSharedMemoryAddr = addr;
    mSharedMemorySize = size;
    mSharedMemoryFd = fd;
    mBufferManager = manager;

    shareMemoryByMemFdAck();
    LOGI("send ShareMemoryReady reply >>>>>>>>>>");
}

void ShmServerSession::writData(const uint8_t* msg, uint32_t len)
{
    if (!mBufferManager || !msg || len == 0) return;

    auto* mgr  = mBufferManager;
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

    if (first == INVALID_INDEX) return;

    uint32_t capacity = queue->capacity;
    uint32_t tail = queue->tail.load(std::memory_order_acquire);
    uint32_t head = queue->head.load(std::memory_order_acquire);
    uint32_t next_tail = (tail + 1) % capacity;

    if (next_tail == head) {
        LOGE("Queue full! (head=%u, tail=%u)", head, tail);
        // 回滚 slice
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

    uint32_t prev_flags = queue->workingFlags.fetch_or(WORKING_FLAG, std::memory_order_acq_rel);
    if ((prev_flags & WORKING_FLAG) == 0) {
        dataSync();
        LOGI("Sent SyncEvent after successful enqueue (tail=%u)", next_tail);
    }
}

void ShmServerSession::dataSync() {
    if (mClientFd != -1) {
        ShmIpcMessage dataSyncMsg;
        auto type_byte = static_cast<uint8_t>(ShmProtocolType::SyncEvent);
        auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE;
        dataSyncMsg.header = ShmIpcMessageHeader(type_byte, length , 0);
        mShmProtocolHandler->sendShmMessage(mClientFd, dataSyncMsg);
    }
}

void ShmServerSession::shareMemoryByMemFdAck() {
    if (mClientFd != -1) {
        ShmIpcMessage dataSyncMsg;
        auto type_byte = static_cast<uint8_t>(ShmProtocolType::ShareMemoryReady);
        auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE;
        dataSyncMsg.header = ShmIpcMessageHeader(type_byte, length , 0);
        mShmProtocolHandler->sendShmMessage(mClientFd, dataSyncMsg);
    }
}

void ShmServerSession::exchangeMetaData(ShmMetadata shmMetadata) {
    if (metaDataIsValid(shmMetadata)) {
        LOGI(">>>>>>>>>> receive exchangeMetaData");
        LOGI("meta: shmSize=%u sliceSize=%u eventQueueSize=%u",
             shmMetadata.shmSize, shmMetadata.sliceSize, shmMetadata.eventQueueSize);
        metaData = shmMetadata;
    }

    if(metaDataIsValid(metaData)) {
        ShmIpcMessage msg;
        msg.payload.resize(sizeof(ShmMetadata));
        memcpy(msg.payload.data(), &metaData, sizeof(ShmMetadata));
        auto type = static_cast<uint8_t>(ShmProtocolType::ExchangeMetadata);
        auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE + msg.payload.size();
        msg.header = ShmIpcMessageHeader(type, length, msg.fds.size());
        mShmProtocolHandler->sendShmMessage(mClientFd, msg);
        LOGI("send exchangeMetaData reply >>>>>>>>>>");
    }
}

void ShmServerSession::handleShareMemoryByMemfd() {
    LOGI(">>>>>>>>>> receive ShareMemoryByMemfd");
    ShmIpcMessage msg;
    auto type = static_cast<uint8_t>(ShmProtocolType::AckReadyRecvFD);
    auto length = SHM_SERVER_PROTOCOL_HEAD_SIZE;
    msg.header = ShmIpcMessageHeader(type, length, msg.fds.size());
    mShmProtocolHandler->sendShmMessage(mClientFd, msg);
    LOGI("send AckReadyRecvFD reply >>>>>>>>>>");
}
