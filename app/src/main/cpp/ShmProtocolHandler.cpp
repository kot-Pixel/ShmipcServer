#include "ShmProtocolHandler.h"
#include "ShmBufferManager.h"
#include "ShmServerSession.h"
#include "ShmMetadata.h"

void ShmProtocolHandler::handleShareMemoryByMemfd(const ShmIpcMessage &message) {
    if (shmSessionCtx) {
        auto* session = static_cast<ShmServerSession*>(shmSessionCtx);
        session->handleShareMemoryByMemfd();
    }
}


bool ShmProtocolHandler::receiveProtocolHeader(int fd, uint8_t* header, std::vector<int>& received_fds)
{
    received_fds.clear();

    size_t total = 0;

    char cmsgbuf[CMSG_SPACE(sizeof(int) * SHM_SERVER_MAX_UDS_PROTOCOL_FD)];
    memset(cmsgbuf, 0, sizeof(cmsgbuf));

    bool control_parsed = false;

    while (total < SHM_SERVER_PROTOCOL_HEAD_SIZE)
    {
        struct iovec iov;
        iov.iov_base = header + total;
        iov.iov_len  = SHM_SERVER_PROTOCOL_HEAD_SIZE - total;

        struct msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgbuf;
        msg.msg_controllen = sizeof(cmsgbuf);

        ssize_t n = recvmsg(fd, &msg, 0);

        if (n == 0) {
            LOGI("peer closed");
            return false;
        }

        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;

            LOGE("rec msg error: %s", strerror(errno));
            return false;
        }
        if (!control_parsed) {
            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            while (cmsg) {
                if (cmsg->cmsg_level == SOL_SOCKET &&
                    cmsg->cmsg_type == SCM_RIGHTS)
                {
                    int* fds = (int*)CMSG_DATA(cmsg);
                    size_t fd_count =
                            (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);

                    for (size_t i = 0; i < fd_count; i++) {
                        received_fds.push_back(fds[i]);
                    }
                }
                cmsg = CMSG_NXTHDR(&msg, cmsg);
            }
            control_parsed = true;
        }
        total += n;
    }
    return true;
}


bool ShmProtocolHandler::receiveProtocolPayload(int fd, char* buf, size_t len)
{
    size_t total = 0;

    while (total < len)
    {
        ssize_t n = read(fd, buf + total, len - total);

        if (n > 0)
            total += n;
        else if (n == 0)
            return false;
        else
        {
            if (errno == EINTR)
                continue;

            return false;
        }
    }

    return true;
}

void ShmProtocolHandler::exchangeMetaData(const ShmIpcMessage &message) {
    if (message.payload.size() < sizeof(ShmMetadata)) {
        LOGE("payload too small");
        return;
    }
    ShmMetadata meta{};
    memcpy(&meta, message.payload.data(), sizeof(ShmMetadata));
    if (!metaDataIsValid(meta)) {
        LOGE("invalid metadata");
        return;
    }
    if (shmSessionCtx) {
        auto* session = static_cast<ShmServerSession*>(shmSessionCtx);
        session->exchangeMetaData(meta);
    }
}


void ShmProtocolHandler::syncEvent(const ShmIpcMessage &message) {
    LOGI("handler syncEvent");
    if (shmSessionCtx) {
        auto* session = static_cast<ShmServerSession*>(shmSessionCtx);
        session->dataSync();
    }
}


void ShmProtocolHandler::shareMemoryByMemfd(const ShmIpcMessage &message) {
    LOGI("handler shareMemoryByMemfd");
    if (message.fds.empty()) {
        LOGE("No fd received");
        return;
    }

    int shm_fd = message.fds[0];

    if (shm_fd < 0) {
        LOGE("Invalid fd");
        return;
    }

    struct stat st{};
    if (fstat(shm_fd, &st) < 0) {
        LOGE("fstat failed");
        return;
    }
    size_t size = st.st_size;

    void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        LOGE("memory map failed");
        return;
    }

    ShmBufferManager* mgr = init_shm_buffer_manager(addr, size);

    LOGD("ShmBufferManager: io_queue capacity=%u, slice_count=%u\n",
           mgr->io_queue.capacity,
           mgr->buffer_list.slice_count);

    LOGD("ShmBufferSlice size %d", sizeof(ShmBufferSlice));

    if (shmSessionCtx) {
        auto* session = static_cast<ShmServerSession*>(shmSessionCtx);
        session->onSharedMemoryReady(addr, size, shm_fd, mgr);
    }

    LOGI("Server mapped memory fd at %p, size=%zu", addr, size);
}

int ShmProtocolHandler::sendShmMessage(int socketFd, const ShmIpcMessage &message) {
    struct msghdr msg{};
    struct iovec iov[2];

    auto headerBytes = message.header.serialize();

    iov[0].iov_base = (void *) headerBytes.data();
    iov[0].iov_len = headerBytes.size();

    iov[1].iov_base = (void *) message.payload.data();
    iov[1].iov_len = message.payload.size();

    int iovcnt = message.payload.empty() ? 1 : 2;

    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;

    char cmsgbuf[CMSG_SPACE(sizeof(int) * message.fds.size())];
    memset(cmsgbuf, 0, sizeof(cmsgbuf));

    if (!message.fds.empty()) {
        msg.msg_control = cmsgbuf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * message.fds.size());

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * message.fds.size());

        memcpy(CMSG_DATA(cmsg), message.fds.data(),
               sizeof(int) * message.fds.size());
    } else {
        msg.msg_control = nullptr;
        msg.msg_controllen = 0;
    }

    ssize_t total = 0;
    ssize_t expected = 0;

    for (int i = 0; i < iovcnt; ++i) {
        expected += iov[i].iov_len;
    }

    bool control_sent = false;

    while (total < expected) {
        ssize_t n = sendmsg(socketFd, &msg, 0);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;

            LOGE("send msg failed errno=%d (%s)", errno, strerror(errno));
            return -1;
        }

        if (n == 0) {
            LOGE("send msg returned 0");
            return -1;
        }

        total += n;

        if (!control_sent) {
            msg.msg_control = nullptr;
            msg.msg_controllen = 0;
            control_sent = true;
        }

        ssize_t remaining = n;
        for (int i = 0; i < iovcnt; ++i) {
            if (remaining >= (ssize_t) iov[i].iov_len) {
                remaining -= iov[i].iov_len;
                iov[i].iov_base = (char *) iov[i].iov_base + iov[i].iov_len;
                iov[i].iov_len = 0;
            } else {
                iov[i].iov_base = (char *) iov[i].iov_base + remaining;
                iov[i].iov_len -= remaining;
                break;
            }
        }
        msg.msg_iov = iov;
    }

    return 0;
}
