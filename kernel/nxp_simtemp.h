/**
 * @file    nxp_simtemp.h
 * @author  Omar Mendiola
 * @brief   Main header file for the NXP simtemp driver.
 * @version 0.1
 * @date    2025-10-14
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef NXP_SIMTEMP_H_
#define NXP_SIMTEMP_H_

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include "simtemp_debug.h"

/**
 * @brief Enumeration for the simulation modes.
 */
enum simtemp_mode {
    SIMTEMP_MODE_NORMAL,
    SIMTEMP_MODE_NOISY,
    SIMTEMP_MODE_RAMP,
    SIMTEMP_MODE_MAX,
};

/**
 * @brief Structure for holding driver statistics.
 */
struct simtemp_stats {
    u64 updates;
    u64 alerts;
    u64 errors;
};

/**
 * @brief Main device structure for the simulated temperature sensor.
 */
struct simtemp_dev {
    struct device *dev;         /* Pointer to the underlying device */
    struct miscdevice misc_dev;    /* misc device's device struct */
    struct mutex lock;          /* Mutex for protecting shared data */
    struct timer_list timer;    /* Kernel timer for the simulator */

    /* Configuration */
    u32 sampling_ms;            /* Update period in milliseconds */
    s32 threshold_mc;           /* Alert threshold in milli-Celsius */
    enum simtemp_mode mode;    /* Simulation mode */

    /* State */
    s32 temp_mc;                /* Current temperature in milli-Celsius */
    struct simtemp_stats stats;/* Statistics counters */
};



#endif /* NXP_SIMTEMP_H_ */