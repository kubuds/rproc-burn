#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"
#include "serial.h"

int serial_open(const char *path)
{
    struct termios tio;
    int fd;

    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        log_err("open %s: %s", path, strerror(errno));
        return -1;
    }
    if (tcgetattr(fd, &tio) < 0)
    {
        log_err("tcgetattr %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(CSTOPB | CRTSCTS);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) < 0)
    {
        log_err("tcsetattr %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}
