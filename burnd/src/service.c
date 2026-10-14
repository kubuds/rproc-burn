#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "crc32.h"
#include "loader.h"
#include "log.h"
#include "protocol.h"
#include "service.h"

/* Give up on a transfer when no byte arrives for this long. */
#define RECEIVE_TIMEOUT_MS 20000

enum state
{
    ST_IDLE,      /* collecting an 8-byte command frame */
    ST_RECEIVING, /* firmware bytes are flowing into the temp file */
    ST_TAIL,      /* all firmware bytes are in; collecting the 8-byte tail */
};

struct service
{
    int fd;
    enum state state;

    uint8_t frame[FRAME_SIZE]; /* command or tail frame being collected */
    size_t frame_len;

    int out;            /* temp file receiving the firmware, or -1 */
    uint32_t expected;  /* bytes announced by UPDATE */
    uint32_t received;
    uint32_t crc;
};

static const char *cmd_name(uint16_t cmd)
{
    switch (cmd)
    {
    case NONE:
        return "NONE";
    case CONNECT:
        return "CONNECT";
    case STOP:
        return "STOP";
    case START:
        return "START";
    case UPDATE:
        return "UPDATE";
    default:
        return "UNKNOWN";
    }
}

static int send_reply(struct service *s, uint16_t cmd, uint16_t status)
{
    frame_reply r;
    ssize_t n;

    r.magic[0] = FRAME_MAGIC0;
    r.magic[1] = FRAME_MAGIC1;
    r.cmd = cmd;
    r.status = status;
    r.err = status;
    log_info("<- %s status %u", cmd_name(cmd), status);

    n = write(s->fd, &r, sizeof(r));
    if (n != (ssize_t)sizeof(r))
    {
        log_err("write reply: %s", n < 0 ? strerror(errno) : "short write");
        return -1;
    }
    return 0;
}

/* Drop whatever transfer is in flight. The temp file is removed unless a
 * successful commit already renamed it away (unlink then fails, harmlessly). */
static void abort_transfer(struct service *s)
{
    if (s->out >= 0)
    {
        close(s->out);
        s->out = -1;
    }
    if (s->state != ST_IDLE)
        unlink(burnd_cfg.temp_path);
    s->state = ST_IDLE;
    s->frame_len = 0;
}

static int begin_transfer(struct service *s, uint32_t length)
{
    if (length == 0)
    {
        log_err("UPDATE with zero length");
        return TK_DATA_ERR;
    }
    if (loader_prepare_dir() < 0)
        return TK_OPTS_ERR;

    s->out = open(burnd_cfg.temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (s->out < 0)
    {
        log_err("create %s: %s", burnd_cfg.temp_path, strerror(errno));
        return TK_OPTS_ERR;
    }
    s->expected = length;
    s->received = 0;
    s->crc = CRC32_INIT;
    s->state = ST_RECEIVING;
    s->frame_len = 0;
    log_info("receiving %u bytes into %s", length, burnd_cfg.temp_path);
    return TK_OK;
}

/* The whole image plus its tail arrived: verify and move it into place. */
static int finish_transfer(struct service *s)
{
    frame_tail tail;
    int ret;

    memcpy(&tail, s->frame, sizeof(tail));
    if (close(s->out) < 0)
    {
        log_err("close %s: %s", burnd_cfg.temp_path, strerror(errno));
        s->out = -1;
        return TK_OPTS_ERR;
    }
    s->out = -1;

    if (tail.marker != FRAME_TAIL_MARKER)
    {
        log_err("bad tail marker %08x", tail.marker);
        return TK_DATA_ERR;
    }
    if (tail.crc32 != s->crc)
    {
        log_err("crc32 mismatch: host %08x, received %08x", tail.crc32, s->crc);
        return TK_DATA_ERR;
    }
    ret = loader_commit(burnd_cfg.temp_path);
    if (ret < 0)
        return TK_OPTS_ERR;
    log_info("firmware %s updated (%u bytes, crc32 %08x)", burnd_cfg.firmware_path,
             s->received, s->crc);
    return TK_OK;
}

static void handle_command(struct service *s)
{
    frame_command c;
    int status;

    memcpy(&c, s->frame, sizeof(c));
    s->frame_len = 0;
    log_info("-> %s length %u", cmd_name(c.cmd), c.length);

    switch (c.cmd)
    {
    case NONE:
    case CONNECT:
        send_reply(s, c.cmd, TK_OK);
        break;
    case STOP:
        send_reply(s, c.cmd, loader_stop() < 0 ? TK_OPTS_ERR : TK_OK);
        break;
    case START:
        send_reply(s, c.cmd, loader_start() < 0 ? TK_OPTS_ERR : TK_OK);
        break;
    case UPDATE:
        status = begin_transfer(s, c.length);
        send_reply(s, c.cmd, status);
        break;
    default:
        log_err("unknown command %u", c.cmd);
        send_reply(s, ERROR, TK_OK);
        break;
    }
}

/*
 * Collect bytes into s->frame, discarding anything before the AA 55 magic
 * so a stray byte cannot desynchronise the exchange. Returns the number of
 * bytes consumed from data.
 */
static size_t collect_command(struct service *s, const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        if (s->frame_len == 0)
        {
            if (b == FRAME_MAGIC0)
                s->frame[s->frame_len++] = b;
        }
        else if (s->frame_len == 1)
        {
            if (b == FRAME_MAGIC1)
                s->frame[s->frame_len++] = b;
            else if (b != FRAME_MAGIC0)
                s->frame_len = 0;
        }
        else
        {
            s->frame[s->frame_len++] = b;
            if (s->frame_len == FRAME_SIZE)
                return i + 1;
        }
    }
    return len;
}

static void consume(struct service *s, const uint8_t *data, size_t len)
{
    while (len > 0)
    {
        size_t n;

        switch (s->state)
        {
        case ST_IDLE:
            n = collect_command(s, data, len);
            if (s->frame_len == FRAME_SIZE)
                handle_command(s);
            break;

        case ST_RECEIVING:
            n = s->expected - s->received;
            if (n > len)
                n = len;
            if (write(s->out, data, n) != (ssize_t)n)
            {
                log_err("write %s: %s", burnd_cfg.temp_path, strerror(errno));
                abort_transfer(s);
                send_reply(s, UPDATE, TK_OPTS_ERR);
                return;
            }
            s->crc = crc32_update(s->crc, data, n);
            s->received += n;
            if (s->received == s->expected)
                s->state = ST_TAIL;
            break;

        case ST_TAIL:
            n = FRAME_SIZE - s->frame_len;
            if (n > len)
                n = len;
            memcpy(s->frame + s->frame_len, data, n);
            s->frame_len += n;
            if (s->frame_len == FRAME_SIZE)
            {
                int status = finish_transfer(s);
                abort_transfer(s); /* releases the temp file on failure, resets state */
                send_reply(s, UPDATE, status);
            }
            break;

        default:
            n = len;
            break;
        }
        data += n;
        len -= n;
    }
}

int service_run(int fd)
{
    struct service s;
    struct pollfd pfd;
    uint8_t buf[4096];

    memset(&s, 0, sizeof(s));
    s.fd = fd;
    s.out = -1;
    s.state = ST_IDLE;

    pfd.fd = fd;
    pfd.events = POLLIN;

    for (;;)
    {
        int timeout = s.state == ST_IDLE ? -1 : RECEIVE_TIMEOUT_MS;
        int rc = poll(&pfd, 1, timeout);
        ssize_t n;

        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            log_err("poll: %s", strerror(errno));
            abort_transfer(&s);
            return -1;
        }
        if (rc == 0)
        {
            log_err("transfer timed out after %u of %u bytes", s.received, s.expected);
            abort_transfer(&s);
            send_reply(&s, UPDATE, TK_TIMEOUT);
            continue;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            log_err("serial port closed");
            abort_transfer(&s);
            return -1;
        }

        n = read(fd, buf, sizeof(buf));
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            log_err("read: %s", strerror(errno));
            abort_transfer(&s);
            return -1;
        }
        if (n == 0)
            continue;
        consume(&s, buf, (size_t)n);
    }
}
