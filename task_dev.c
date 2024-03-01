#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <uapi/asm-generic/errno-base.h>

#include "ringbuffer.h"

MODULE_DESCRIPTION("Symbolic driver for communication of two processes");
MODULE_AUTHOR("Ismagil Iskakov");
MODULE_LICENSE("GPL v2");

#define TDEV_NAME "task_dev"
#define TDEV_CLASS_NAME "task_devClass"


#define TLOG_PREF TDEV_NAME ": "

static dev_t tdev_nr;
static struct class* tdev_class;
static struct cdev tdev_cdev;

static struct tringbuffer trb;

static int tdev_open(struct inode *inode, struct file *file) {
    pr_info(TLOG_PREF "Device open was called\n");
    return 0;
}

static ssize_t tdev_read(struct file *file, char __user *buf, size_t bytes, loff_t *offset) {
    pr_info(TLOG_PREF "Device read was called, bytes: %lu\n", bytes);
    size_t rr = tringbuffer_read(&trb, buf, bytes);
    return rr;
}

static ssize_t tdev_write(struct file *file, const char __user *buf, size_t bytes, loff_t *offset) {
    pr_info(TLOG_PREF "Device write was called, bytes: %lu, offt: %lld\n", bytes, *offset);
    size_t wr = tringbuffer_write(&trb, buf, bytes);
    if (wr < bytes) return -ENOSPC;
    return wr;
}

static int tdev_release(struct inode *inode, struct file *file) {
    pr_info(TLOG_PREF "Device release was called\n");
    return 0;
}

static struct file_operations tfops = {
    .owner = THIS_MODULE,
    .open = tdev_open,
    .release = tdev_release,
    .read = tdev_read,
    .write = tdev_write
};

static char* tdev_devnode(const struct device *dev, umode_t *mode) {
    if (mode == NULL) {
        return NULL;
    }
    *mode = 0666;
    return NULL;
}

static int __init mod_init(void) {
    trb = tringbuffer_init(24);
    if (trb.capacity == 0) {
        pr_err(TLOG_PREF "Ringbuffer failed to init\n");
        return -1;
    }

    if (alloc_chrdev_region(&tdev_nr, 0, 1, TDEV_NAME) < 0) {
        pr_err(TLOG_PREF "Device nr was not registered\n");
        return -1;
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
    
    pr_info(TLOG_PREF "Loaded module\n");
    return 0;

AddError:
    device_destroy(tdev_class, tdev_nr);
DeviceFileError:
    class_destroy(tdev_class);
ClassError:
    unregister_chrdev_region(tdev_nr, 1);
    return -1;
}

static void __exit mod_exit(void) {
    tringbuffer_deinit(&trb);
    cdev_del(&tdev_cdev);
    device_destroy(tdev_class, tdev_nr);
    class_destroy(tdev_class);
    unregister_chrdev_region(tdev_nr, 1);

    pr_info(TLOG_PREF "Removed module\n");
}

module_init(mod_init);
module_exit(mod_exit);
