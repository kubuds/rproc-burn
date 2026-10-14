#ifndef __LOG_H__
#define __LOG_H__

/*
 * Logging shared by the daemon and the one-shot load mode.
 *
 * Errors always go to stderr. When log_init() enabled syslog (daemon mode)
 * they are also sent there. Info messages are emitted only when verbose.
 */

extern void log_init(int use_syslog, int verbose);
extern void log_err(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* __LOG_H__ */
