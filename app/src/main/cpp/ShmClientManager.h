//
// Created by Tom on 2026/3/12.
//

#ifndef SHMIPCC_SHMCLIENTMANAGER_H
#define SHMIPCC_SHMCLIENTMANAGER_H

#include <atomic>
#include <vector>
#include <mutex>
#include "ShmClientSession.h"

#include <thread>
#include <unistd.h>
#include <iostream>
#include <map>

class ShmClientManager {
private :
    std::mutex mClientMutex;
    std::map<int, std::unique_ptr<ShmClientSession>> mShmClientSessionMap;

public :
    bool createShmClientSession(int fd);

    void cleanAllShmClient();
};


#endif //SHMIPCC_SHMCLIENTMANAGER_H
