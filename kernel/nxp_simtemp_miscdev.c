/**
 * @file    nxp_simtemp_miscdev.c
 * @author  Omar Mendiola
 * @brief   Miscellaneous device implementation for the NXP simtemp driver.
 * Provides a /dev/simtemp interface for userspace to read sensor data.
 * @version 0.1
 * @date    2025-10-14
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "nxp_simtemp.h"


static int simtemp_open(struct inode *inode, struct file *filp)
{
    struct simtemp_dev *simtemp;
    struct miscdevice *misc_device;

    // misc_device = container_of(inode->i_cdev, struct miscdevice, this_device->cdev);
    // simtemp = container_of(misc_device, struct simtemp_dev, misc_dev);

    misc_device = filp->private_data;
    simtemp = container_of(misc_device, struct simtemp_dev, misc_dev);

    debug_pr_addr("simtemp_open: inode", inode);
    debug_pr_addr("simtemp_open: misc_device", misc_device);
    debug_pr_addr("simtemp_open: simtemp", simtemp);

    filp->private_data = simtemp;
    return 0;
}

/**
 * @brief Read function for the misc device.
 *
 * Called when a userspace application reads from /dev/simtemp. It copies
 * the latest temperature value to the user's buffer.
 *
 * @param filp Pointer to the file structure.
 * @param buf Userspace buffer to copy data to.
 * @param count Number of bytes to read.
 * @param offp Pointer to the file offset.
 * @return ssize_t The number of bytes read, or a negative error code.
 */
static ssize_t simtemp_read(struct file *filp, char __user *buf,
                             size_t count, loff_t *offp)
{
struct simtemp_dev *simtemp = filp->private_data;
    s32 temp_mc;
    int ret;

    if (*offp > 0)
        return 0;

    if (count < sizeof(s32))
        return -EINVAL;

    mutex_lock(&simtemp->lock);
    temp_mc = simtemp->temp_mc;
    mutex_unlock(&simtemp->lock);

    ret = copy_to_user(buf, &temp_mc, sizeof(s32));
    if (ret)
        return -EFAULT;

    *offp += sizeof(s32);

    return sizeof(s32);
}

static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .open = simtemp_open,
    .read = simtemp_read,
};

/**
 * @brief Initializes the miscellaneous device.
 *
 * Registers the misc device, sets its parent to the platform device,
 * and stores a pointer to its struct device for later use with sysfs.
 *
 * @param dev Pointer to the main simtemp_dev structure.
 * @return int 0 on success, or a negative error code on failure.
 */
int nxp_simtemp_miscdev_init(struct simtemp_dev *simtemp)
{
    int ret;
    struct miscdevice *misc_device = &simtemp->misc_dev;

    misc_device->minor = MISC_DYNAMIC_MINOR;
    misc_device->name = "simtemp";
    misc_device->fops = &simtemp_fops;
    /* Set the parent device before registering */
    misc_device->parent = simtemp->dev; //platform device's is the parent

     printk(KERN_INFO "Registering misc device: %s\n", misc_device->name); 

    ret = misc_register(misc_device);
    if (ret) {
        pr_err("Failed to register misc device\n");
        return ret;
    }

    /* Store per-device context for retrieval via dev_get_drvdata(). */
    /* Tied to misc->this_device lifecycle. */
    dev_set_drvdata(misc_device->this_device, simtemp);

    return 0;
}

/**
 * @brief Deinitializes the miscellaneous device.
 */
void nxp_simtemp_miscdev_exit(struct simtemp_dev *simtemp)
{
    misc_deregister(&simtemp->misc_dev);
}