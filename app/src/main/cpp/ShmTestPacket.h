//
// Created by kotlinx on 2026/3/24.
//

#ifndef SHMIPCC_SHMTESTPACKET_H
#define SHMIPCC_SHMTESTPACKET_H

#include <stdint.h>

struct ShmTestPacket {
    uint32_t seq;
    uint32_t len;
    uint8_t  data[0];
};

#endif //SHMIPCC_SHMTESTPACKET_H
