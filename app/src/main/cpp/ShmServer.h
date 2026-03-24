#ifndef SHMIPCC_SHMSERVER_H
#define SHMIPCC_SHMSERVER_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <memory>

#include <stdio.h>
#include <string.h>
#include "ShmServerSessionManager.h"
#include "ShmLogger.h"
#include "ShmServerConfig.h"

class ShmServer {
    int server_fd = -1;
    struct sockaddr_un addr;
    std::unique_ptr<ShmServerSessionManager> mShmClientManager;
public:
    ShmServer() : server_fd(-1), addr{},
                  mShmClientManager(std::unique_ptr<ShmServerSessionManager>(new ShmServerSessionManager())) {
    }
    ~ShmServer() {
        if (server_fd != -1) {
            close(server_fd);
            server_fd = -1;
        }
    }
    bool initShmServer();

    std::vector<ShmServerSession*> getAllShmServerSessionMap();
};


#endif //SHMIPCC_SHMSERVER_H
