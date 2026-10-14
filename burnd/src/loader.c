#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "loader.h"
#include "log.h"

/* How long to wait for a state transition, in 100 ms steps. */
#define STATE_WAIT_STEPS 30

static int read_state(char *buf, size_t n)
{
    int fd = open(burnd_cfg.sysfs_state, O_RDONLY);
    ssize_t len;

    if (fd < 0)
    {
        log_err("open %s: %s", burnd_cfg.sysfs_state, strerror(errno));
        return -1;
    }
    len = read(fd, buf, n - 1);
    close(fd);
    if (len < 0)
    {
        log_err("read %s: %s", burnd_cfg.sysfs_state, strerror(errno));
        return -1;
    }
    buf[len] = '\0';
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    return 0;
}

static int write_attr(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    ssize_t len = strlen(value);

    if (fd < 0)
    {
        log_err("open %s: %s", path, strerror(errno));
        return -1;
    }
    if (write(fd, value, len) != len)
    {
        log_err("write %s to %s: %s", value, path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int wait_state(const char *want)
{
    char state[32];
    int i;

    for (i = 0; i < STATE_WAIT_STEPS; i++)
    {
        if (read_state(state, sizeof(state)) < 0)
            return -1;
        if (strcmp(state, want) == 0)
            return 0;
        usleep(100 * 1000);
    }
    log_err("remote core did not become %s (state %s)", want, state);
    return -1;
}

int loader_stop(void)
{
    char state[32];

    if (read_state(state, sizeof(state)) < 0)
        return -1;
    if (strcmp(state, "running") != 0)
        return 0;
    if (write_attr(burnd_cfg.sysfs_state, "stop") < 0)
        return -1;
    if (wait_state("offline") < 0)
        return -1;
    log_info("remote core stopped");
    return 0;
}

int loader_start(void)
{
    if (write_attr(burnd_cfg.sysfs_firmware, burnd_cfg.firmware_name) < 0)
        return -1;
    if (write_attr(burnd_cfg.sysfs_state, "start") < 0)
        return -1;
    if (wait_state("running") < 0)
        return -1;
    log_info("remote core running %s", burnd_cfg.firmware_name);
    return 0;
}

int loader_prepare_dir(void)
{
    struct stat st;

    if (stat(burnd_cfg.firmware_dir, &st) == 0)
        return 0;
    if (mkdir(burnd_cfg.firmware_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH) < 0)
    {
        log_err("mkdir %s: %s", burnd_cfg.firmware_dir, strerror(errno));
        return -1;
    }
    return 0;
}

int loader_commit(const char *tmp)
{
    if (access(burnd_cfg.firmware_path, F_OK) == 0)
    {
        remove(burnd_cfg.backup_path);
        if (rename(burnd_cfg.firmware_path, burnd_cfg.backup_path) < 0)
            log_err("backup %s: %s", burnd_cfg.firmware_path, strerror(errno));
    }
    if (rename(tmp, burnd_cfg.firmware_path) < 0)
    {
        log_err("rename %s to %s: %s", tmp, burnd_cfg.firmware_path, strerror(errno));
        return -1;
    }
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    char buf[64 * 1024];
    ssize_t n;
    int in, out;
    int ret = 0;

    in = open(src, O_RDONLY);
    if (in < 0)
    {
        log_err("open %s: %s", src, strerror(errno));
        return -1;
    }
    out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0)
    {
        log_err("create %s: %s", dst, strerror(errno));
        close(in);
        return -1;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0)
    {
        if (write(out, buf, n) != n)
        {
            log_err("write %s: %s", dst, strerror(errno));
            ret = -1;
            break;
        }
    }
    if (n < 0)
    {
        log_err("read %s: %s", src, strerror(errno));
        ret = -1;
    }
    close(in);
    if (close(out) < 0)
    {
        log_err("close %s: %s", dst, strerror(errno));
        ret = -1;
    }
    return ret;
}

int loader_install(const char *src)
{
    struct stat st_src, st_dst;

    if (loader_prepare_dir() < 0)
        return -1;

    if (stat(src, &st_src) < 0)
    {
        log_err("%s: %s", src, strerror(errno));
        return -1;
    }

    /* Loading the file that is already in place: nothing to copy. */
    if (stat(burnd_cfg.firmware_path, &st_dst) == 0 &&
        st_src.st_ino == st_dst.st_ino && st_src.st_dev == st_dst.st_dev)
        return 0;

    /* Copy to a temp file first so a failed copy never clobbers the image. */
    if (copy_file(src, burnd_cfg.temp_path) < 0)
    {
        unlink(burnd_cfg.temp_path);
        return -1;
    }
    return loader_commit(burnd_cfg.temp_path);
}

int loader_load(const char *src)
{
    if (loader_stop() < 0)
        return -1;
    if (loader_install(src) < 0)
        return -1;
    return loader_start();
}
