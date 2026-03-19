//
// Created by Tom on 2026/3/12.
//

#ifndef SHMIPCC_SHMSERVERSESSIONMANAGER_H
#define SHMIPCC_SHMSERVERSESSIONMANAGER_H

#include <atomic>
#include <vector>
#include <mutex>
#include "ShmServerSession.h"

#include <thread>
#include <unistd.h>
#include <iostream>
#include <map>

/**
 * Shm Server session Manager
 *
 * manage all shm ipc into connect session
 */
class ShmServerSessionManager {
private :
    std::mutex mClientMutex;
    std::map<int, std::unique_ptr<ShmServerSession>> mShmClientSessionMap;

public :
    bool createShmClientSession(int fd);

    void cleanAllShmClient();
};


#endif //SHMIPCC_SHMSERVERSESSIONMANAGER_H
