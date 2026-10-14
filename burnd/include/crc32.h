#ifndef __CRC32_H__
#define __CRC32_H__

#include <stddef.h>
#include <stdint.h>

/* IEEE 802.3 CRC-32 as computed by zlib's crc32(), fed incrementally. */
extern uint32_t crc32_update(uint32_t crc, const void *buf, size_t len);

#define CRC32_INIT 0

#endif /* __CRC32_H__ */
