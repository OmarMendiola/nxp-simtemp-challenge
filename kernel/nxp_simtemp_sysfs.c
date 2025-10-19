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
	if (!simtemp) return -ENODEV;
	debug_pr_addr("show: dev    ", dev);
	debug_pr_addr("show: simtemp", simtemp);

    mutex_lock(&simtemp->lock);
    u32 sampling_ms = simtemp->sampling_ms;
    mutex_unlock(&simtemp->lock);

	return sysfs_emit(buf, "%u\n", sampling_ms);
}

static ssize_t sampling_ms_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!simtemp) return -ENODEV;

	ret = kstrtoul(buf, 10, &val);
	if (ret) {
		pr_err("simtemp: Invalid input for sampling_ms: '%s'\n", buf);
		return ret;
	}

	/* --- VALIDATION --- */
	if (val < SIMTEMP_SAMPLING_MS_MIN || val > SIMTEMP_SAMPLING_MS_MAX) {
		pr_warn("simtemp: sampling_ms value %lu out of range [%u-%u]\n",
		        val, SIMTEMP_SAMPLING_MS_MIN, SIMTEMP_SAMPLING_MS_MAX);
		return -EINVAL; /* Invalid argument */
	}
	/* --- END VALIDATION --- */

	mutex_lock(&simtemp->lock);
	simtemp->sampling_ms = (u32)val;
	/* Optionally: Restart timer immediately with new value, or let the next callback handle it */
	/* mod_timer(&simtemp->timer, jiffies + msecs_to_jiffies(simtemp->sampling_ms)); */
	mutex_unlock(&simtemp->lock);

	debug_dbg("sampling_ms set to %u\n", simtemp->sampling_ms);
	return count;
}
static DEVICE_ATTR_RW(sampling_ms);

/* --- threshold_mc attribute --- */
static ssize_t threshold_mc_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
	if (!simtemp) return -ENODEV;

    mutex_lock(&simtemp->lock);
    s32 threshold_mc = simtemp->threshold_mc;
    mutex_unlock(&simtemp->lock);
	return sysfs_emit(buf, "%d\n", threshold_mc);
}

static ssize_t threshold_mc_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
	long val;
	int ret;

	if (!simtemp) return -ENODEV;

	ret = kstrtol(buf, 10, &val);
	if (ret) {
		pr_err("simtemp: Invalid input for threshold_mc: '%s'\n", buf);
		return ret;
	}

	/* --- VALIDATION --- */
	if (val < SIMTEMP_THRESHOLD_MC_MIN || val > SIMTEMP_THRESHOLD_MC_MAX) {
		pr_warn("simtemp: threshold_mc value %ld out of range [%d-%d]\n",
		        val, SIMTEMP_THRESHOLD_MC_MIN, SIMTEMP_THRESHOLD_MC_MAX);
		return -EINVAL; /* Invalid argument */
	}
	/* --- END VALIDATION --- */

	mutex_lock(&simtemp->lock);
	simtemp->threshold_mc = (s32)val;
	mutex_unlock(&simtemp->lock);

	debug_dbg("threshold_mc set to %d\n", simtemp->threshold_mc);
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
	if (!simtemp) return -ENODEV;

    mutex_lock(&simtemp->lock);
    enum simtemp_mode mode = simtemp->mode;
    mutex_unlock(&simtemp->lock);
	/* Check bounds in case mode is somehow corrupted */
	if (mode >= SIMTEMP_MODE_MAX || mode < 0)
		return sysfs_emit(buf, "invalid\n");
	return sysfs_emit(buf, "%s\n", simtemp_modes[mode]);
}

static ssize_t mode_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf, size_t count)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
	int i;

	if (!simtemp) return -ENODEV;

	for (i = 0; i < SIMTEMP_MODE_MAX; i++) {
		if (sysfs_streq(buf, simtemp_modes[i])) {
			mutex_lock(&simtemp->lock);
			simtemp->mode = i;
			mutex_unlock(&simtemp->lock);
			debug_dbg("mode set to %s\n", simtemp_modes[i]);
			return count;
		}
	}

	pr_warn("simtemp: Invalid mode value: '%s'. Valid modes: normal, noisy, ramp\n", buf);
	return -EINVAL;
}
static DEVICE_ATTR_RW(mode);

/* --- stats attribute --- */
static ssize_t stats_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    struct simtemp_dev *simtemp = dev_get_drvdata(dev);
	u64 updates, alerts, errors; /* Use local vars to minimize lock time */

	if (!simtemp) return -ENODEV;

	/* Get stats atomically */
	mutex_lock(&simtemp->lock);
	updates = simtemp->stats.updates;
	alerts = simtemp->stats.alerts;
	errors = simtemp->stats.errors;
	mutex_unlock(&simtemp->lock);

	return sysfs_emit(buf, "updates=%llu alerts=%llu errors=%llu\n",
	                  updates, alerts, errors);
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