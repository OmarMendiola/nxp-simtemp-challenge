/**
 * @file    nxp_simtemp_sysfs.c
 * @author  Omar Mendiola
 * @brief   Sysfs interface for the NXP simtemp driver.
 * Exposes attributes for configuration and statistics.
 * @version 0.1
 * @date    2025-10-14
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "nxp_simtemp.h"

/* --- sampling_ms attribute --- */
static ssize_t sampling_ms_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    debug_pr_addr("show: dev    ", dev);
    debug_pr_addr("show: simtemp", simtemp);

    return sysfs_emit(buf, "%u\n", simtemp->sampling_ms);
}

static ssize_t sampling_ms_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    unsigned long val;
    int ret = kstrtoul(buf, 10, &val);
    if (ret)
        return ret;

    mutex_lock(&simtemp->lock);
    simtemp->sampling_ms = val;
    mutex_unlock(&simtemp->lock);

    return count;
}
static DEVICE_ATTR_RW(sampling_ms);

/* --- threshold_mc attribute --- */
static ssize_t threshold_mc_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    return sysfs_emit(buf, "%d\n", simtemp->threshold_mc);
}

static ssize_t threshold_mc_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    long val;
    int ret = kstrtol(buf, 10, &val);
    if (ret)
        return ret;

    mutex_lock(&simtemp->lock);
    simtemp->threshold_mc = val;
    mutex_unlock(&simtemp->lock);

    return count;
}
static DEVICE_ATTR_RW(threshold_mc);


/* --- mode attribute --- */
static const char * const simtemp_modes[] = {
    [SIMTEMP_MODE_NORMAL] = "normal",
    [SIMTEMP_MODE_NOISY] = "noisy",
    [SIMTEMP_MODE_RAMP] = "ramp",
};

static ssize_t mode_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    return sysfs_emit(buf, "%s\n", simtemp_modes[simtemp->mode]);
}

static ssize_t mode_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    int i;

    for (i = 0; i < SIMTEMP_MODE_MAX; i++) {
        if (sysfs_streq(buf, simtemp_modes[i])) {
            mutex_lock(&simtemp->lock);
            simtemp->mode = i;
            mutex_unlock(&simtemp->lock);
            return count;
        }
    }

    return -EINVAL;
}
static DEVICE_ATTR_RW(mode);

/* --- stats attribute --- */
static ssize_t stats_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
    return sysfs_emit(buf, "updates=%llu alerts=%llu errors=%llu\n",
                      simtemp->stats.updates,
                      simtemp->stats.alerts,
                      simtemp->stats.errors);
}
static DEVICE_ATTR_RO(stats);


/* --- Attribute Group --- */
static struct attribute *simtemp_attrs[] = {
    &dev_attr_sampling_ms.attr,
    &dev_attr_threshold_mc.attr,
    &dev_attr_mode.attr,
    &dev_attr_stats.attr,
    NULL,
};

static struct attribute_group simtemp_attr_group = {
    .attrs = simtemp_attrs,
};

/**
 * @brief Initializes the sysfs interface for the device.
 *
 * @param simtemp Pointer to the simtemp device structure.
 * @return int 0 on success, or a negative error code on failure.
 */
int nxp_simtemp_sysfs_init(struct simtemp_dev *simtemp)
{
    // Create the sysfs group under the misc device's device kobject
    debug_pr_addr("Sysfs init: miscdevice", &simtemp->misc_dev);
    debug_pr_addr("Sysfs init: device of miscdevice", simtemp->misc_dev.this_device);
    debug_pr_addr("Sysfs init: kobj", &simtemp->misc_dev.this_device->kobj);
    return sysfs_create_group(&simtemp->misc_dev.this_device->kobj, &simtemp_attr_group);
}

/**
 * @brief Deinitializes the sysfs interface for the device.
 *
 * @param simtemp Pointer to the simtemp device structure.
 */
void nxp_simtemp_sysfs_exit(struct simtemp_dev *simtemp)
{
    sysfs_remove_group(&simtemp->misc_dev.this_device->kobj, &simtemp_attr_group);
}