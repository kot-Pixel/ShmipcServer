#ifndef SHMIPCC_SHMSERVER_H
#define SHMIPCC_SHMSERVER_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <memory>

#include <stdio.h>
#include <string.h>
#include "ShmClientManager.h"
#include "ShmLogger.h"
#include "ShmServerConfig.h"

class ShmServer {
    int server_fd = -1;
    struct sockaddr_un addr;
    std::unique_ptr<ShmClientManager> mShmClientManager;

    void createShmClientSession();
public:
    ShmServer() : server_fd(-1), addr{},
                  mShmClientManager(std::unique_ptr<ShmClientManager>(new ShmClientManager())) {
    }
    ~ShmServer() {
        if (server_fd != -1) {
            close(server_fd);
            server_fd = -1;
        }
    }
    bool initShmServer();
};


#endif //SHMIPCC_SHMSERVER_H
