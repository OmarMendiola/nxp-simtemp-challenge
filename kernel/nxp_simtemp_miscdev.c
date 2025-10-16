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

static struct simtemp_dev *misc_simtemp_dev;

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
    struct simtemp_dev *simtemp = misc_simtemp_dev;
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
    .read = simtemp_read,
};

static struct miscdevice simtemp_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "simtemp",
    .fops = &simtemp_fops,
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
    misc_simtemp_dev = simtemp;

    /* Set the parent device before registering */
    simtemp_miscdev.parent = simtemp->dev;

    ret = misc_register(&simtemp_miscdev);
    if (ret) {
        pr_err("Failed to register misc device\n");
        return ret;
    }

    /* Store the created device pointer for sysfs association */
    simtemp->misc_dev = simtemp_miscdev.this_device;
    return 0;
}

/**
 * @brief Deinitializes the miscellaneous device.
 */
void nxp_simtemp_miscdev_exit(void)
{
    misc_deregister(&simtemp_miscdev);
}