#include "ShmProtocolHandler.h"
#include "ShmBufferManager.h"
#include "ShmServerSession.h"

uint32_t ShmProtocolHandler::parsePayloadLength(uint8_t header[SHM_SERVER_PROTOCOL_HEAD_SIZE   ]) {
    uint32_t totalLength = 0;

    totalLength |= (static_cast<uint8_t>(header[0]) << 24);
    totalLength |= (static_cast<uint8_t>(header[1]) << 16);
    totalLength |= (static_cast<uint8_t>(header[2]) << 8);
    totalLength |= (static_cast<uint8_t>(header[3]));

    if (totalLength < SHM_SERVER_PROTOCOL_HEAD_SIZE) {
        return 0;
    }
    return totalLength - SHM_SERVER_PROTOCOL_HEAD_SIZE;
}

void ShmProtocolHandler::handleShmIpcProtocol(char type, std::vector<char> vector) {
    uint8_t typeValue = static_cast<uint8_t>(type);
    ShmProtocolType protocolType = static_cast<ShmProtocolType>(typeValue);

    switch (protocolType) {
        case ShmProtocolType::ExchangeMetadata:
            // 处理元信息交换
            break;

        case ShmProtocolType::ShareMemoryByFilePath:
            // 处理文件路径共享内存
            break;

        case ShmProtocolType::ShareMemoryByMemfd:
            //client will send shm fd.
            handleShareMemoryByMemfd();
            break;

        case ShmProtocolType::AckReadyRecvFD:
            // 处理接收FD确认
            break;

        case ShmProtocolType::AckShareMemory:
            // 处理共享内存完成确认
            break;

        case ShmProtocolType::SyncEvent:
            // 处理同步事件
            break;

        case ShmProtocolType::FallbackData:
            // 处理回退数据
            break;

        default:
            // 未知类型
            break;
    }

}

void ShmProtocolHandler::handleShareMemoryByMemfd() {

}


bool ShmProtocolHandler::receiveProtocolHeader(int fd, uint8_t* header, std::vector<int>& received_fds)
{

    struct iovec iov;
    iov.iov_base = header;
    iov.iov_len  = SHM_SERVER_PROTOCOL_HEAD_SIZE;

    /* create max receive fd msg
     * max receive fd set to SHM_SERVER_MAX_UDS_PROTOCOL_FD
     * */
    char cmsgbuf[CMSG_SPACE(sizeof(int) * SHM_SERVER_MAX_UDS_PROTOCOL_FD)];

    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    ssize_t n = recvmsg(fd, &msg, 0);
    if (n <= 0) {
        LOGI("unix domain socket receive msg size not correct");
        return false;
    }

    received_fds.clear();

    uint8_t fdCount = header[SHM_SERVER_PROTOCOL_HEAD_SIZE - 1];
    bool existFd = (fdCount != 0);

    if (existFd) {
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        while (cmsg) {
            if (cmsg->cmsg_level == SOL_SOCKET &&
                cmsg->cmsg_type == SCM_RIGHTS)
            {
                int* fds = (int*)CMSG_DATA(cmsg);
                size_t fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                for (size_t i = 0; i < fd_count; i++) {
                    received_fds.push_back(fds[i]);
                }
            }
            cmsg = CMSG_NXTHDR(&msg, cmsg);
        }
    } else {
        LOGI("unix domain socket not include fd.");
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
    LOGI("handler exchangeMetaData");
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

    iov[0].iov_base = (void*)headerBytes.data();
    iov[0].iov_len  = headerBytes.size();

    iov[1].iov_base = (void*)message.payload.data();
    iov[1].iov_len  = message.payload.size();

    msg.msg_iov = iov;
    msg.msg_iovlen = message.payload.empty() ? 1 : 2;

    char cmsgbuf[CMSG_SPACE(sizeof(int) * message.fds.size())];

    if (!message.fds.empty())
    {

        LOGD("send fds start");

        msg.msg_control = cmsgbuf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * message.fds.size());

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_RIGHTS;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * message.fds.size());

        memcpy(CMSG_DATA(cmsg), message.fds.data(),
               sizeof(int) * message.fds.size());

        LOGD("send fds end");
    }

    ssize_t n = sendmsg(socketFd, &msg, 0);

    if (n < 0)
    {
        perror("sendmsg");
        return -1;
    }

    return 0;
}

