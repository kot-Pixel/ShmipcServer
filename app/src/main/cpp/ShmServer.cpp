//
// Created by Tom on 2026/3/12.
//

#include "ShmServer.h"


bool ShmServer::initShmServer() {
    int ret = 0;
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    strcpy(addr.sun_path + 1, SHM_SERVER_DEFAULT_NAME);
    int len = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(SHM_SERVER_DEFAULT_NAME);
    ret = bind(server_fd, (struct sockaddr*)&addr, len);

    if (ret != 0) {
        LOGE("failure bind shm server unix domain socket");
        goto exit;
    }
    ret = listen(server_fd, SHM_SERVER_MAX_PROGRESS_COUNT);
    if (ret != 0) {
        LOGE("failure start shm server listener");
        goto exit;
    }

    LOGI("bind and listen success..");

    while (true) {
        int clientFd = accept(server_fd, nullptr, nullptr);
        if (clientFd != -1) {
            ret = mShmClientManager->createShmClientSession(clientFd);
            if (ret) {
                LOGI("### create shm client session success ###");
            } else {
                LOGE("### create shm client session fail ###");
            }
        } else {
            LOGE("client fd is error");
        }
    }
exit:
    mShmClientManager->cleanAllShmClient();
    return false;
}

std::vector<ShmServerSession*> ShmServer::getAllShmServerSessionMap() {
    return mShmClientManager->getAllSessions();
}
