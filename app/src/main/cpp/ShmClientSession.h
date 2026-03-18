#ifndef SHMIPCC_SHMCLIENTSESSION_H
#define SHMIPCC_SHMCLIENTSESSION_H

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <sys/epoll.h>

#include "ShmProtocolHandler.h"
#include "ShmMessageQueue.h"
#include "ShmBufferManager.h"


class ShmClientSession {

public:
    int mClientFd = -1;
    void startRunReadThreadLoop();
    void stopRunReadThreadLoop();

    // write data to remote
    void writData(const uint8_t* msg, uint32_t len);

    void dataSync();

    ShmClientSession() {
        mShmProtocolHandler = std::unique_ptr<ShmProtocolHandler>(new ShmProtocolHandler(this));
    }

    ~ShmClientSession() {
        stopRunReadThreadLoop();
    }

    void onSharedMemoryReady(void* addr, size_t size, int fd, ShmBufferManager* manager);

private:
    ShmMessageQueue mMessageQueue;

    std::atomic<bool> mShmReadThreadRunning{};
    std::unique_ptr<std::thread> mShmReadThread;
    std::unique_ptr<std::thread> mShmProgressThread;
    std::unique_ptr<ShmProtocolHandler> mShmProtocolHandler;


    void* mSharedMemoryAddr = nullptr;
    size_t mSharedMemorySize = 0;
    int mSharedMemoryFd = -1;
    ShmBufferManager* mBufferManager = nullptr;


    void clientUdsReader();
    void messageProcessor();
};

#endif //SHMIPCC_SHMCLIENTSESSION_H
