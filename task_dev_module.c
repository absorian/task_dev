#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <asm-generic/errno-base.h>

#include "task_dev.h"
#include "ringbuffer.h"

MODULE_DESCRIPTION("Symbolic driver for communication of two processes");
MODULE_AUTHOR("Ismagil Iskakov");
MODULE_LICENSE("GPL v2");

#define TDEV_NAME "task_dev"
#define TDEV_CLASS_NAME "task_devClass"

static unsigned long long rb_size = 256;

module_param(rb_size, ullong, 0);
MODULE_PARM_DESC(rb_size, "The size of ringbuffer");


#define TLOG_PREF TDEV_NAME ": "

struct tdev_fd_data {
    spinlock_t lock;
    bool nonblock;
};

static wait_queue_head_t tdev_wq;

static dev_t tdev_nr;
static struct class* tdev_class;
static struct cdev tdev_cdev;

static struct tringbuffer *trb;

static atomic_long_t tdev_read_cnt;
static atomic_long_t tdev_write_cnt;

static struct tdev_ioc_info tdev_info;
static spinlock_t tdev_info_lock;

static long tdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    void* __user argp = (void* __user)arg;
    struct tdev_fd_data* dat = file->private_data;

    switch(cmd) {
    case TDEV_IOC_GETINFO:
        unsigned long inf_fl;
        spin_lock_irqsave(&tdev_info_lock, inf_fl);
        if(copy_to_user((struct tdev_ioc_info *) argp, &tdev_info, sizeof(struct tdev_ioc_info))) {
            spin_unlock_irqrestore(&tdev_info_lock, inf_fl);
            return -EFAULT;
        }
        spin_unlock_irqrestore(&tdev_info_lock, inf_fl);

        break;
    case TDEV_IOC_NONBLOCK: {
        int on = *(int *)argp;
        pr_info(TLOG_PREF "Device ioctl TDEV_IOC_NONBLOCK with value %d\n", on);
        unsigned long flags;
        spin_lock_irqsave(&dat->lock, flags);
        dat->nonblock = on;
        spin_unlock_irqrestore(&dat->lock, flags);
        break;
    }
    default:
        return -ENOTTY;
    }
    
    return 0;
}

static int tdev_open(struct inode *inode, struct file *file) {
    pr_info(TLOG_PREF "Device open was called\n");

    file->private_data = kmalloc(sizeof(struct tdev_fd_data), GFP_KERNEL);
    if (file->private_data == NULL) return -ENOMEM;

    struct tdev_fd_data* dat = file->private_data;
    spin_lock_init(&dat->lock);
    dat->nonblock = false;

    unsigned int acc_mode = file->f_flags & O_ACCMODE;
    if (acc_mode == O_RDONLY) {
        atomic_long_inc(&tdev_read_cnt);
    } else if (acc_mode == O_WRONLY) {
        atomic_long_inc(&tdev_write_cnt);
    } else if (acc_mode == O_RDWR) {
        atomic_long_inc(&tdev_write_cnt);
        atomic_long_inc(&tdev_read_cnt);
    } else goto FlagError;

    return 0;

    FlagError:
    kfree(file->private_data);
    return -1;
}

static ssize_t tdev_read(struct file *file, char __user *buf, size_t bytes, loff_t *offset) {
    pr_info(TLOG_PREF "Device read was called, bytes: %lu\n", bytes);
    struct tdev_fd_data* dat = file->private_data;

    unsigned long inf_fl;
    spin_lock_irqsave(&tdev_info_lock, inf_fl);
    tdev_info.last_read.pid = task_pid_nr(current);
    tdev_info.last_read.uid = current_uid().val;
    tdev_info.last_read.timestamp = ktime_get_real_ns();
    spin_unlock_irqrestore(&tdev_info_lock, inf_fl);
    
    size_t b = tringbuffer_stored(trb);
    if (b) {
        b = tringbuffer_read(trb, buf, bytes);
        wake_up(&tdev_wq);
        return b;
    }
    
    long writers = atomic_long_read(&tdev_write_cnt);
    if (!writers) return 0;

    unsigned long flags;
    spin_lock_irqsave(&dat->lock, flags);
    if (dat->nonblock) {
        spin_unlock_irqrestore(&dat->lock, flags);
        return -EAGAIN;
    }
    spin_unlock_irqrestore(&dat->lock, flags);

    if (wait_event_interruptible(tdev_wq, tringbuffer_stored(trb) != 0 || !(writers = atomic_long_read(&tdev_write_cnt))
        ) == -ERESTARTSYS)
        return 0;
    if (!writers) return 0;

    b = tringbuffer_read(trb, buf, bytes);
    wake_up(&tdev_wq);
    return b;
}

static ssize_t tdev_write(struct file *file, const char __user *buf, size_t bytes, loff_t *offset) {
    pr_info(TLOG_PREF "Device write was called, bytes: %lu, offt: %lld\n", bytes, *offset);
    struct tdev_fd_data* dat = file->private_data;

    unsigned long inf_fl;
    spin_lock_irqsave(&tdev_info_lock, inf_fl);
    tdev_info.last_write.pid = task_pid_nr(current);
    tdev_info.last_write.uid = current_uid().val;
    tdev_info.last_write.timestamp = ktime_get_real_ns();
    spin_unlock_irqrestore(&tdev_info_lock, inf_fl);
    
    size_t b = tringbuffer_available(trb);
    if (b >= bytes) {
        b = tringbuffer_write(trb, buf, bytes);
        wake_up(&tdev_wq);
        return b;
    }

    unsigned long flags;
    spin_lock_irqsave(&dat->lock, flags);
    if (dat->nonblock) {
        spin_unlock_irqrestore(&dat->lock, flags);
        if (!b || bytes <= tringbuffer_capacity(trb)) {
            return -EAGAIN;
        }
        b = tringbuffer_write(trb, buf, bytes);
        wake_up(&tdev_wq);
        return b;
    }
    spin_unlock_irqrestore(&dat->lock, flags);

    long readers = atomic_long_read(&tdev_read_cnt);

    // not in 'man write(2)' but it shuts up 'bigstuff > /dev/task_dev' when empty and lonely
    if (!readers && !b) return -ENOSPC;
    
    size_t written = 0;
    while(1) {
        b = tringbuffer_write(trb, buf + written, bytes - written);
        wake_up(&tdev_wq);
        written += b;
        if (bytes == written) return bytes;

        if (wait_event_interruptible(tdev_wq, tringbuffer_available(trb) != 0 || !(readers = atomic_long_read(&tdev_read_cnt))
            ) == -ERESTARTSYS)
            return written;
        if (!readers) return written;
    }
}

static int tdev_release(struct inode *inode, struct file *file) {
    pr_info(TLOG_PREF "Device release was called\n");
    struct tdev_fd_data* dat = file->private_data;

    unsigned int acc_mode = file->f_flags & O_ACCMODE;
    if (acc_mode == O_RDONLY) {
        atomic_long_dec(&tdev_read_cnt);
    } else if (acc_mode == O_WRONLY) {
        atomic_long_dec(&tdev_write_cnt);
    } else if (acc_mode == O_RDWR) {
        atomic_long_dec(&tdev_write_cnt);
        atomic_long_dec(&tdev_read_cnt);
    }
    wake_up(&tdev_wq);
    
    kfree(dat);

    return 0;
}

static struct file_operations tfops = {
    .owner = THIS_MODULE,
    .open = tdev_open,
    .release = tdev_release,
    .read = tdev_read,
    .write = tdev_write,
    .unlocked_ioctl = tdev_ioctl
};

static char* tdev_devnode(const struct device *dev, umode_t *mode) {
    if (mode == NULL) {
        return NULL;
    }
    *mode = 0666;
    return NULL;
}

static int __init mod_init(void) {
    trb = tringbuffer_init(rb_size);
    if (trb == NULL) {
        pr_err(TLOG_PREF "Ringbuffer failed to init\n");
        return -1;
    }

    if (alloc_chrdev_region(&tdev_nr, 0, 1, TDEV_NAME) < 0) {
        pr_err(TLOG_PREF "Device nr was not registered\n");
        goto DeviceNrError;
    }
    pr_info(TLOG_PREF "Device nr was allocated (%d:%d)\n", MAJOR(tdev_nr), MINOR(tdev_nr));
    
    tdev_class = class_create(TDEV_CLASS_NAME);
    if (tdev_class == NULL) {
        pr_err(TLOG_PREF "Device class was not created\n");
        goto ClassError;
    }
    tdev_class->devnode = tdev_devnode;
    
    if (device_create(tdev_class, NULL, tdev_nr, NULL, TDEV_NAME) == NULL) {
        pr_err(TLOG_PREF "Device was not created\n");
        goto DeviceFileError;
    }

    cdev_init(&tdev_cdev, &tfops);
    if (cdev_add(&tdev_cdev, tdev_nr, 1) < 0) {
        pr_err(TLOG_PREF "Device was not added to the kernel\n");
        goto AddError;
    }

    atomic_long_set(&tdev_read_cnt, 0);
    atomic_long_set(&tdev_write_cnt, 0);
    
    spin_lock_init(&tdev_info_lock);
    
    init_waitqueue_head(&tdev_wq);

    pr_info(TLOG_PREF "Loaded module\n");
    return 0;

AddError:
    device_destroy(tdev_class, tdev_nr);
DeviceFileError:
    class_destroy(tdev_class);
ClassError:
    unregister_chrdev_region(tdev_nr, 1);
DeviceNrError:
    tringbuffer_deinit(trb);
    return -1;
}

static void __exit mod_exit(void) {
    tringbuffer_deinit(trb);
    cdev_del(&tdev_cdev);
    device_destroy(tdev_class, tdev_nr);
    class_destroy(tdev_class);
    unregister_chrdev_region(tdev_nr, 1);

    pr_info(TLOG_PREF "Removed module\n");
}

module_init(mod_init);
module_exit(mod_exit);
