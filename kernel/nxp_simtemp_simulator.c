/**
 * @file    nxp_simtemp_simulator.c
 * @author  Omar Mendiola
 * @brief   Temperature simulator implementation using a kernel timer.
 * @version 0.1
 * @date    2025-10-14
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/slab.h>

#include "nxp_simtemp.h"

/**
 * @brief The timer callback function.
 *
 * This function is executed periodically to generate a new temperature value,
 * update statistics, and check for threshold alerts.
 *
 * @param t Pointer to the timer_list structure.
 */
static void simtemp_timer_callback(struct timer_list *t)
{
struct simtemp_dev *simtemp = from_timer(simtemp, t, timer);
	s32 new_temp;
	s64 current_ns;
	u32 flags = SIMTEMP_SAMPLE_FLAG_NEW; // Always mark as new

	/* Get current monotonic timestamp BEFORE generating temp */
	current_ns = ktime_get_ns();

	/* --- Temperature Generation Logic --- */
	// Uses simtemp->latest_sample.temp_mc for ramp mode continuity
	switch (simtemp->mode) {
	case SIMTEMP_MODE_NOISY:
		get_random_bytes(&new_temp, sizeof(new_temp));
		new_temp = 25000 + (new_temp % 5000);
		break;
	case SIMTEMP_MODE_RAMP:
		/* Read the previous temp atomically before calculation */
		mutex_lock(&simtemp->lock);
		new_temp = simtemp->latest_sample.temp_mc + 100;
		mutex_unlock(&simtemp->lock);
		if (new_temp > 100000)
			new_temp = 0;
		break;
	case SIMTEMP_MODE_NORMAL:
	default:
		new_temp = 27500;
		break;
	}

	/* Check threshold */
	if (new_temp > simtemp->threshold_mc) {
		flags |= SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI;
	}

	/* --- Update Shared State --- */
	mutex_lock(&simtemp->lock);

	simtemp->latest_sample.timestamp_ns = current_ns;
	simtemp->latest_sample.temp_mc = new_temp;
	simtemp->latest_sample.flags = flags;

	simtemp->stats.updates++;
	if (flags & SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI) {
		simtemp->stats.alerts++;
	}

	/* Mark sample as available */
	simtemp->new_sample_available = true;

	mutex_unlock(&simtemp->lock);
	/* --- End Update Shared State --- */


	/* Wake up any waiting readers */
	wake_up_interruptible(&simtemp->read_wq);
    debug_dbg("Timer: Woke up readers for new sample\n");

	/* Reschedule the timer */
	mod_timer(&simtemp->timer, jiffies + msecs_to_jiffies(simtemp->sampling_ms));

	/* Debug message (optional) */
	/* debug_dbg("Timer: New sample generated (%lld ns, %d mC, flags=0x%x)\n",
		   current_ns, new_temp, flags); */
}

/**
 * @brief Initializes the simulator.
 *
 * Sets up the initial state and starts the kernel timer.
 *
 * @param dev Pointer to the main simtemp_dev structure.
 * @return int Always returns 0.
 */
int nxp_simtemp_simulator_init(struct simtemp_dev *simtemp)
{
    /* Set default values */
    simtemp->sampling_ms = 1000;
    simtemp->threshold_mc = 50000;
    simtemp->mode = SIMTEMP_MODE_NORMAL;
    simtemp->latest_sample.temp_mc = 25000; /* Initial temperature 25 C */
	simtemp->latest_sample.timestamp_ns = ktime_get_ns(); /* Initial timestamp */
	simtemp->latest_sample.flags = 0; /* Initial flags */
	simtemp->new_sample_available = false; /* No new sample initially */

    /* Initialize the wait queue */
	init_waitqueue_head(&simtemp->read_wq);

    /* Setup and start the timer */
    timer_setup(&simtemp->timer, simtemp_timer_callback, 0);
    mod_timer(&simtemp->timer, jiffies + msecs_to_jiffies(simtemp->sampling_ms));

    debug_dbg("Simulator initialized. Timer started.\n");

    return 0;
}

/**
 * @brief Deinitializes the simulator.
 *
 * Stops the kernel timer.
 *
 * @param dev Pointer to the main simtemp_dev structure.
 */
void nxp_simtemp_simulator_exit(struct simtemp_dev *simtemp)
{
    del_timer_sync(&simtemp->timer);
}