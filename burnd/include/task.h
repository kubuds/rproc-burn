#ifndef __TASK_H__
#define __TASK_H__

enum
{
    FIRMWARE_OFF = 0,
    FIRMWARE_ON,
};


extern int echo_firmware();
extern int ctrl_firmware(int flag);
extern int run_task(enum TASK_CMD cmd, uint32_t param);

#endif