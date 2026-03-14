//
// Created by Tom on 2026/3/12.
//

#ifndef SHMIPCC_SHMPROTOCOLHANDLER_H
#define SHMIPCC_SHMPROTOCOLHANDLER_H

#include <vector>
#include <sys/socket.h>
#include <unistd.h>

#include "ShmLogger.h"
#include "ShmServerConfig.h"



enum class ShmProtocolType : uint8_t {
    ExchangeMetadata = 1,        // 协议协商，交换元信息
    ShareMemoryByFilePath = 2,   // 通过文件路径映射共享内存
    ShareMemoryByMemfd = 3,       // 通过 memfd 映射共享内存
    AckReadyRecvFD = 4,           // 已做好准备接收 memfd
    AckShareMemory = 5,           // 完成共享内存映射
    SyncEvent = 6,                 // 同步事件
    FallbackData = 7               // 共享内存不足时的回退数据
};

class ShmProtocolHandler {

public:
    bool receiveProtocolHeader(int fd, uint8_t* header, std::vector<int>& received_fds);
    bool receiveProtocolPayload(int fd, char* buf, size_t len);

    uint32_t parsePayloadLength(uint8_t header[SHM_SERVER_PROTOCOL_HEAD_SIZE]);

    void handleShmIpcProtocol(char type, std::vector<char> payload);

    //handle client send shm file description
    void handleShareMemoryByMemfd();
};


#endif //SHMIPCC_SHMPROTOCOLHANDLER_H
