#ifndef __CONFIG_H__
#define __CONFIG_H__

/*
 * Runtime configuration of burnd. Every path used to be a compile-time
 * constant; they are now command line options so the same binary can serve
 * other boards (different serial gadget, remoteproc index or firmware name).
 * The defaults reproduce the historical behaviour.
 */

#define BURND_DEFAULT_DEVICE "/dev/ttyGS0"
#define BURND_DEFAULT_RPROC "/sys/class/remoteproc/remoteproc0"
#define BURND_DEFAULT_FIRMWARE_DIR "/lib/firmware"
#define BURND_DEFAULT_FIRMWARE "arduino.elf"

#define BURND_PATH_MAX 256

struct burnd_config
{
    char device[BURND_PATH_MAX];        /* serial port burntool talks to */
    char rproc_dir[BURND_PATH_MAX];     /* sysfs directory of the remote core */
    char firmware_dir[BURND_PATH_MAX];  /* where the kernel looks for firmware */
    char firmware_name[BURND_PATH_MAX]; /* name written to rproc "firmware" */

    /* derived from the fields above by config_init() */
    char firmware_path[BURND_PATH_MAX]; /* firmware_dir/firmware_name */
    char backup_path[BURND_PATH_MAX];   /* previous image, <name>_old.<ext> */
    char temp_path[BURND_PATH_MAX];     /* image being received, <name>.tmp */
    char sysfs_firmware[BURND_PATH_MAX];/* rproc_dir/firmware */
    char sysfs_state[BURND_PATH_MAX];   /* rproc_dir/state */

    /* "burnd load FILE": install FILE and restart the core, then exit. */
    const char *load_file;

    int verbose; /* -v: log every frame and state change */
};

extern struct burnd_config burnd_cfg;

/*
 * Parse the command line into burnd_cfg. Returns 0 to continue, 1 when the
 * program should exit successfully (help was printed), -1 on a bad option.
 */
extern int config_init(int argc, char **argv);

#endif /* __CONFIG_H__ */
