#include <unistd.h>

#include "config.h"
#include "loader.h"
#include "log.h"
#include "serial.h"
#include "service.h"

int main(int argc, char **argv)
{
    int ret;
    int fd;

    ret = config_init(argc, argv);
    if (ret != 0)
        return ret < 0 ? 2 : 0;

    if (burnd_cfg.load_file != NULL)
    {
        log_init(0, burnd_cfg.verbose);
        return loader_load(burnd_cfg.load_file) == 0 ? 0 : 1;
    }

    log_init(1, burnd_cfg.verbose);

    fd = serial_open(burnd_cfg.device);
    if (fd < 0)
        return 1;
    log_info("listening on %s, remoteproc %s, firmware %s", burnd_cfg.device,
             burnd_cfg.rproc_dir, burnd_cfg.firmware_path);

    /* Boot whatever was burned last time, as the original daemon did. */
    if (access(burnd_cfg.firmware_path, F_OK) == 0)
    {
        if (loader_start() == 0)
            log_info("auto-started %s", burnd_cfg.firmware_name);
    }
    else
    {
        log_info("no firmware at %s, nothing to auto-start", burnd_cfg.firmware_path);
    }

    ret = service_run(fd);
    close(fd);
    return ret < 0 ? 1 : 0;
}
