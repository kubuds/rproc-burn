#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "config.h"

struct burnd_config burnd_cfg;

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage: %s [options]\n"
            "\n"
            "Receive firmware over a serial port and load it on the remote core\n"
            "through the Linux remoteproc interface.\n"
            "\n"
            "  -d, --device PATH        serial device to listen on (default %s)\n"
            "  -r, --rproc PATH|NAME    remoteproc sysfs directory, or a name under\n"
            "                           /sys/class/remoteproc (default %s)\n"
            "  -D, --firmware-dir DIR   directory the kernel loads firmware from\n"
            "                           (default %s)\n"
            "  -f, --firmware NAME      firmware file name (default %s)\n"
            "  -h, --help               show this help\n",
            prog, BURND_DEFAULT_DEVICE, BURND_DEFAULT_RPROC,
            BURND_DEFAULT_FIRMWARE_DIR, BURND_DEFAULT_FIRMWARE);
}

static int set_str(char *dst, const char *src)
{
    if (strlen(src) >= BURND_PATH_MAX)
    {
        fprintf(stderr, "burnd: value too long: %s\n", src);
        return -1;
    }
    strcpy(dst, src);
    return 0;
}

/* Join dir and name into dst, refusing to overflow. */
static int join(char *dst, const char *dir, const char *name)
{
    int n = snprintf(dst, BURND_PATH_MAX, "%s/%s", dir, name);
    if (n < 0 || n >= BURND_PATH_MAX)
    {
        fprintf(stderr, "burnd: path too long: %s/%s\n", dir, name);
        return -1;
    }
    return 0;
}

/* arduino.elf -> arduino_old.elf, keeping the historical backup name. */
static int backup_name(char *dst, const char *name)
{
    const char *dot = strrchr(name, '.');
    int n;

    if (dot == NULL || dot == name)
        n = snprintf(dst, BURND_PATH_MAX, "%s_old", name);
    else
        n = snprintf(dst, BURND_PATH_MAX, "%.*s_old%s", (int)(dot - name), name, dot);

    if (n < 0 || n >= BURND_PATH_MAX)
    {
        fprintf(stderr, "burnd: firmware name too long: %s\n", name);
        return -1;
    }
    return 0;
}

static int derive_paths(struct burnd_config *c)
{
    char backup[BURND_PATH_MAX];

    if (backup_name(backup, c->firmware_name) < 0)
        return -1;
    if (join(c->firmware_path, c->firmware_dir, c->firmware_name) < 0)
        return -1;
    if (join(c->backup_path, c->firmware_dir, backup) < 0)
        return -1;
    if (join(c->sysfs_firmware, c->rproc_dir, "firmware") < 0)
        return -1;
    if (join(c->sysfs_state, c->rproc_dir, "state") < 0)
        return -1;
    return 0;
}

int config_init(int argc, char **argv)
{
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"rproc", required_argument, NULL, 'r'},
        {"firmware-dir", required_argument, NULL, 'D'},
        {"firmware", required_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    struct burnd_config *c = &burnd_cfg;
    int opt;

    strcpy(c->device, BURND_DEFAULT_DEVICE);
    strcpy(c->rproc_dir, BURND_DEFAULT_RPROC);
    strcpy(c->firmware_dir, BURND_DEFAULT_FIRMWARE_DIR);
    strcpy(c->firmware_name, BURND_DEFAULT_FIRMWARE);

    while ((opt = getopt_long(argc, argv, "d:r:D:f:h", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'd':
            if (set_str(c->device, optarg) < 0)
                return -1;
            break;
        case 'r':
            if (optarg[0] == '/')
            {
                if (set_str(c->rproc_dir, optarg) < 0)
                    return -1;
            }
            else if (join(c->rproc_dir, "/sys/class/remoteproc", optarg) < 0)
            {
                return -1;
            }
            break;
        case 'D':
            if (set_str(c->firmware_dir, optarg) < 0)
                return -1;
            break;
        case 'f':
            if (strchr(optarg, '/') != NULL)
            {
                fprintf(stderr, "burnd: --firmware takes a file name, not a path; use --firmware-dir\n");
                return -1;
            }
            if (set_str(c->firmware_name, optarg) < 0)
                return -1;
            break;
        case 'h':
            usage(stdout, argv[0]);
            return 1;
        default:
            usage(stderr, argv[0]);
            return -1;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "burnd: unexpected argument: %s\n", argv[optind]);
        usage(stderr, argv[0]);
        return -1;
    }

    return derive_paths(c);
}
