//
// Created by Tom on 2026/3/12.
//

#ifndef SHMIPCC_SHMCLIENTSESSION_H
#define SHMIPCC_SHMCLIENTSESSION_H

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <sys/epoll.h>

#include "ShmProtocolHandler.h"
#include "ShmMessageQueue.h"


class ShmClientSession {

public:
    int mClientFd{};
    void startRunReadThreadLoop();
    void stopRunReadThreadLoop();

    ShmClientSession() {
        mShmProtocolHandler = std::unique_ptr<ShmProtocolHandler>(new ShmProtocolHandler());
    }

    ~ShmClientSession() {
        stopRunReadThreadLoop();
    }
private:
    ShmMessageQueue mMessageQueue;

    std::atomic<bool> mShmReadThreadRunning{};
    std::unique_ptr<std::thread> mShmReadThread;
    std::unique_ptr<std::thread> mShmProgressThread;
    std::unique_ptr<ShmProtocolHandler> mShmProtocolHandler;
    void clientUdsReader();
    void messageProcessor();
};

#endif //SHMIPCC_SHMCLIENTSESSION_H
