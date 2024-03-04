#include <asm-generic/ioctl.h>
#include <linux/types.h>

#ifndef __KERNEL__
#include <sys/types.h>
#endif

struct tdev_acc_record {
    __u64 timestamp;
    pid_t pid;
    uid_t uid;
};

struct tdev_ioc_info {
    struct tdev_acc_record last_read;
    struct tdev_acc_record last_write;
};

#define TDEV_IOC_NONBLOCK _IOW('t', 0x93, int)
#define TDEV_IOC_GETINFO _IOR('t', 0x94, struct tdev_ioc_info)
