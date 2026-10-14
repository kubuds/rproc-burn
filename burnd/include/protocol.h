#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>

/*
 * Frame protocol between burntool (host) and burnd (board). All frames are
 * 8 bytes, integers little-endian. See docs/protocol.md.
 *
 *   command : AA 55 | cmd u16 | length u32
 *   reply   : AA 55 | cmd u16 | status u16 | errno u16
 *   tail    : crc32 u32 | FF FF FF FF
 */

#define FRAME_SIZE 8
#define FRAME_MAGIC0 0xAA
#define FRAME_MAGIC1 0x55
#define FRAME_TAIL_MARKER 0xFFFFFFFFu

enum TASK_ERRNO
{
    TK_OK = 0,
    TK_TIMEOUT,
    TK_START_ERR,
    TK_STOP_ERR,
    TK_OPTS_ERR,
    TK_SCHED_ERR,
    TK_CHECK_ERR,
    TK_DATA_ERR,
};

enum TASK_CMD
{
    NONE = 0,
    CONNECT,
    STOP,
    START,
    UPDATE,
    ERROR,
};

typedef struct __attribute__((packed))
{
    uint8_t magic[2];
    uint16_t cmd;
    uint32_t length;
} frame_command;

typedef struct __attribute__((packed))
{
    uint8_t magic[2];
    uint16_t cmd;
    uint16_t status;
    uint16_t err;
} frame_reply;

typedef struct __attribute__((packed))
{
    uint32_t crc32;
    uint32_t marker;
} frame_tail;

#endif /* __PROTOCOL_H__ */
