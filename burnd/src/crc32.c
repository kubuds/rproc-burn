#include "crc32.h"

static uint32_t table[256];
static int table_ready;

static void make_table(void)
{
    uint32_t c;
    int n, k;

    for (n = 0; n < 256; n++)
    {
        c = (uint32_t)n;
        for (k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
    table_ready = 1;
}

uint32_t crc32_update(uint32_t crc, const void *buf, size_t len)
{
    const uint8_t *p = buf;

    if (!table_ready)
        make_table();
    crc ^= 0xFFFFFFFFu;
    while (len--)
        crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
