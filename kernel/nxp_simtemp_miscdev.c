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


/* --- Wait Queue Condition Macro --- */

/**
 * @brief Checks if a new sample is available under lock.
 * Intended for use as the condition in wait_event_* macros.
 * @param _simtemp Pointer to the struct simtemp_dev.
 * @return True if a new sample is available, false otherwise.
 */
#define is_new_sample_available(_simtemp) \
	({ \
		bool _available; \
		mutex_lock(&(_simtemp)->lock); \
		_available = (_simtemp)->new_sample_available; \
		mutex_unlock(&(_simtemp)->lock); \
		_available; \
	})

static u32 timeout_jiffies;

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
	struct simtemp_sample local_sample; /* Local copy to avoid holding lock during copy_to_user */
	int ret;

	debug_dbg("simtemp_read called, count=%zu, offp=%lld\n", count, *offp);

/* Check if simtemp pointer is valid (set in open) */
	if (!simtemp) {
		pr_err("simtemp: No device context in file private_data! Was open called?\n");
		return -ENODEV;
	}
	debug_pr_addr("simtemp_read: simtemp context", simtemp);


/* We only support reading the whole structure from the beginning */
	if (*offp > 0) {
		debug_dbg("simtemp_read: EOF condition (*offp > 0)\n");
		return 0; /* EOF */
	}

	if (count < sizeof(struct simtemp_sample)) {
		pr_warn("simtemp: Read buffer too small (%zu bytes provided, %zu needed)\n",
			count, sizeof(struct simtemp_sample));
		return -EINVAL; /* Invalid argument */
	}

/* --- Blocking Logic with timeout--- */
	debug_dbg("simtemp_read: Waiting for new sample...\n");
	/* Sleep until simtemp->new_sample_available is true */
	ret = wait_event_interruptible_timeout(simtemp->read_wq, is_new_sample_available(simtemp), timeout_jiffies);
	if (ret < 0) {
		/* Interrupted by signal */
		debug_dbg("simtemp_read: Wait interrupted by signal (ret=%d)\n", ret);
		return -ERESTARTSYS;
	} else if (ret == 0) {
		/* Timeout occurred */
		pr_warn("simtemp: Read timed out after %d ms waiting for new sample\n",
		        SIMTEMP_READ_TIMEOUT_MS);
		return -ETIMEDOUT; /* Return Timeout error */
	}
	/* --- Woken up: ret > 0, new_sample_available is true --- */
	debug_dbg("simtemp_read: Woken up! New sample available.\n");
	/* --- Woken up: new_sample_available is guaranteed true --- */
	debug_dbg("simtemp_read: Woken up! New sample available.\n");


/* --- Critical Section: Get the sample and mark it consumed --- */
mutex_lock(&simtemp->lock);
	if (!simtemp->new_sample_available) {
		/* Race condition check: sample consumed between wake-up and lock */
		mutex_unlock(&simtemp->lock);
		debug_dbg("simtemp_read: Race condition detected, restarting wait.\n");
		/* Consider restarting the wait or returning -EAGAIN */
		/* For simplicity here, we might just return 0 bytes read or an error */
		/* Or better: retry the wait_event call - but this can get complex */
		return -EAGAIN; // Indicate user should try again
	}
	local_sample = simtemp->latest_sample;
	simtemp->new_sample_available = false; /* Mark consumed */
	mutex_unlock(&simtemp->lock);
/* --- End Critical Section --- */


/* Copy the local sample to user space */
	debug_dbg("simtemp_read: Copying sample to user space (size %zu)\n", sizeof(local_sample));
	if (copy_to_user(buf, &local_sample, sizeof(local_sample))) {
		pr_err("simtemp: Failed to copy sample to user space\n");
		return -EFAULT; /* Bad address */
	}

	/* Update file offset to indicate we've read the data */
	*offp += sizeof(local_sample);

	debug_dbg("simtemp_read: Successfully read %zu bytes\n", sizeof(local_sample));
	return sizeof(local_sample); /* Return the number of bytes successfully read */
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

	/*Calculate bloking read timeout in jiffies just once*/
	timeout_jiffies = msecs_to_jiffies(SIMTEMP_READ_TIMEOUT_MS); 

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