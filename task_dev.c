#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define TDEV_MAJOR 12
#define TDEV_NAME "task_dev"


#define TLOG_PREF TDEV_NAME ": "

struct tdev_data {
    struct cdev cdev;

};

static int tdev_open(struct inode *inode, struct file *file) {
    struct tdev_data *data = container_of(inode->i_cdev, struct tdev_data, cdev);
    file->private_data = data;
    
    pr_info(TLOG_PREF "Device open call\n");
    return 0;
}

static ssize_t tdev_read(struct file *file, char __user *buf, size_t bytes, loff_t *offset) {
    struct tdev_data *data = file->private_data;


    pr_info(TLOG_PREF "Device read call\n");
    return 0;
}

static int tdev_release(struct inode *inode, struct file *file) {
    struct tdev_data *data = file->private_data;


    pr_info(TLOG_PREF "Device release call\n");
    return 0;
}

static struct file_operations tfops = {
    .owner = THIS_MODULE,
    .open = tdev_open,
    .release = tdev_release,
    .read = tdev_read,
};

static int __init mod_init(void)
{
    pr_info(TLOG_PREF "Loading module\n");
    int res = register_chrdev(TDEV_MAJOR, TDEV_NAME, &tfops);
    if (res == 0) {
        pr_info(TLOG_PREF "Device registered with numbers %d:%d\n", TDEV_MAJOR, 0);
    } else if (res > 0) {
        pr_info(TLOG_PREF "Device registered with numbers %d:%d\n", res >> 20, res & 0xfffff);
    } else {
        pr_err(TLOG_PREF "Device was not registered\n");
        return -1;
    }
    
    return 0;
}

static void __exit mod_exit(void)
{
    unregister_chrdev(TDEV_MAJOR, TDEV_NAME);
    pr_info(TLOG_PREF "Removing module\n");
}

module_init(mod_init);
module_exit(mod_exit);

/* Macros for declaring module metadata */
MODULE_DESCRIPTION("Symbol driver for communication of two processes");
MODULE_AUTHOR("Ismagil Iskakov");
MODULE_LICENSE("GPL");
