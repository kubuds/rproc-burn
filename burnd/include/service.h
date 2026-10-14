#ifndef __SERVICE_H__
#define __SERVICE_H__

/*
 * The serial burn service: speaks the burntool frame protocol on one fd
 * and drives the loader. Runs until the fd fails. Returns -1 on error.
 */
extern int service_run(int fd);

#endif /* __SERVICE_H__ */
