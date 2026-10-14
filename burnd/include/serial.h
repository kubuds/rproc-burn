#ifndef __SERIAL_H__
#define __SERIAL_H__

/* Open a tty in raw 8N1 mode at 115200 baud. Returns the fd or -1. */
extern int serial_open(const char *path);

#endif /* __SERIAL_H__ */
