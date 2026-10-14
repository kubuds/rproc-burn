#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "log.h"

static int to_syslog;
static int verbose;

void log_init(int use_syslog, int be_verbose)
{
    to_syslog = use_syslog;
    verbose = be_verbose;
    if (to_syslog)
        openlog("burnd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
}

static void emit(int prio, const char *fmt, va_list ap)
{
    va_list copy;

    if (to_syslog)
    {
        va_copy(copy, ap);
        vsyslog(prio, fmt, copy);
        va_end(copy);
    }
    fputs("burnd: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void log_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit(LOG_ERR, fmt, ap);
    va_end(ap);
}

void log_info(const char *fmt, ...)
{
    va_list ap;

    if (!verbose)
        return;
    va_start(ap, fmt);
    emit(LOG_INFO, fmt, ap);
    va_end(ap);
}
