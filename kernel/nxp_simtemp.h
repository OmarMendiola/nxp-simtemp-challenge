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
#include <linux/wait.h>     // Needed for wait_queue_head_t
#include <linux/ktime.h>    // Needed for ktime_get_ns()

#include "simtemp_debug.h"
#include "nxp_simtemp_config.h"

/* --- Flags for simtemp_sample --- */
#define SIMTEMP_SAMPLE_FLAG_NEW             (1 << 0) /* Indicates a fresh sample */
#define SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI    (1 << 1) /* Indicates threshold crossed */
#define SIMTEMP_SAMPLE_FLAG_OUT_OF_RANGE    (2 << 1) /* Indicates threshold crossed */

/* Add more flags as needed */

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
 * @brief Structure for a single temperature sample (binary record).
 */
struct simtemp_sample {
	__u64 timestamp_ns;   /* Monotonic timestamp in nanoseconds */
	__s32 temp_mc;        /* Temperature in milli-Celsius */
	__u32 flags;          /* Status flags (e.g., new, threshold) */
} __attribute__((packed)); /* Ensure no padding */

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

    /* State for blocking read*/
    struct simtemp_sample latest_sample;                /* Current temperature in milli-Celsius */
    bool new_sample_available;                 /* Flag indicating if a new sample is available */
    wait_queue_head_t read_wq;           /* Wait queue for readers */

    struct simtemp_stats stats;/* Statistics counters */
};



#endif /* NXP_SIMTEMP_H_ */