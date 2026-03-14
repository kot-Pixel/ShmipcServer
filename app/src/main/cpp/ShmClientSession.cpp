//
// Created by Tom on 2026/3/12.
//

#include "ShmClientSession.h"
#include "ShmIpcMessage.h"


void ShmClientSession::startRunReadThreadLoop() {
    mShmReadThreadRunning = true;
    mShmReadThread.reset(new std::thread(&ShmClientSession::clientUdsReader, this));
}

void ShmClientSession::stopRunReadThreadLoop() {
    mShmReadThreadRunning = false;
    if (mShmReadThread && mShmReadThread->joinable()) {
        mShmReadThread->join();
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

    LOGI("ShmClient Session Thread Stop");
}