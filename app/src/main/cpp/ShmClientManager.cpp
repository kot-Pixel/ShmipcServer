//
// Created by Tom on 2026/3/12.
//

#include "ShmClientManager.h"

bool ShmClientManager::createShmClientSession(int clientFd) {
    try {
        auto session = std::unique_ptr<ShmClientSession>(new ShmClientSession);
        session->mClientFd = clientFd;
        session->startRunReadThreadLoop();
        {
            std::lock_guard<std::mutex> lock(mClientMutex);
            mShmClientSessionMap[clientFd] = std::move(session);
        }
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void ShmClientManager::cleanAllShmClient() {
    std::lock_guard<std::mutex> lock(mClientMutex);
    for (auto& [fd, session] : mShmClientSessionMap) {
        session->stopRunReadThreadLoop();
    }
    mShmClientSessionMap.clear();
}

