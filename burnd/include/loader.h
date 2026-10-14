#ifndef __LOADER_H__
#define __LOADER_H__

/*
 * Firmware installation and remote core control through the remoteproc
 * sysfs interface, independent of how the image reached the board.
 * Paths come from burnd_cfg. Functions return 0 on success and -1 on
 * failure after writing a one-line reason to stderr.
 */

/* Stop the remote core if it is running and wait until it is offline. */
extern int loader_stop(void);

/* Point remoteproc at the configured firmware name and start the core. */
extern int loader_start(void);

/* Copy src into the firmware directory, keeping the previous image as backup. */
extern int loader_install(const char *src);

/* stop + install + start, the whole "burn" in one call. */
extern int loader_load(const char *src);

#endif /* __LOADER_H__ */
