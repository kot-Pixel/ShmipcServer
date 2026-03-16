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
}

void ShmClientSession::writData(const uint8_t *msg, uint32_t len) {
    if (mBufferManager != nullptr) {

    }
}
