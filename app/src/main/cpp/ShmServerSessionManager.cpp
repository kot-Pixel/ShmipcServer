//
// Created by Tom on 2026/3/12.
//

#include "ShmServerSessionManager.h"

bool ShmServerSessionManager::createShmClientSession(int clientFd) {
    try {
        auto session = std::unique_ptr<ShmServerSession>(new ShmServerSession);
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

void ShmServerSessionManager::cleanAllShmClient() {
    std::lock_guard<std::mutex> lock(mClientMutex);
    for (auto& [fd, session] : mShmClientSessionMap) {
        session->stopRunReadThreadLoop();
    }
    mShmClientSessionMap.clear();
}

