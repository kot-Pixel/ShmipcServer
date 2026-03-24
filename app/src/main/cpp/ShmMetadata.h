#ifndef SHMIPCCLIEN_SHMMETADATA_H
#define SHMIPCCLIEN_SHMMETADATA_H

#include <stdint.h>

constexpr uint32_t SHM_MAGIC = 0x53484D49; // "SHMI"
constexpr int8_t SHM_VERSION = 1;

#pragma pack(push, 1)
struct ShmMetadata {
    uint32_t magic;
    uint16_t version;

    uint32_t shmSize;
    uint32_t sliceSize;
    uint32_t eventQueueSize;
    uint32_t sliceInvalidNext;

    uint32_t flags;
};
#pragma pack(pop)

inline bool metaDataIsValid(const ShmMetadata& meta) {
    if (meta.magic != SHM_MAGIC) return false;
    if (meta.version < SHM_VERSION) return false;

    if (meta.shmSize == 0) return false;
    if (meta.sliceSize == 0) return false;
    if (meta.eventQueueSize == 0) return false;
    if (meta.sliceInvalidNext == 0) return false;

    return true;
}

#endif //SHMIPCCLIEN_SHMMETADATA_H
