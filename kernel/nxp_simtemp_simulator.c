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
	struct simtemp_sample sample_temp;
	struct simtemp_stats stats_temp;
	s32 threshold;
	enum simtemp_mode mode;
	u32 sampling_ms;

	
	/*Get all the context safety*/
	mutex_lock(&simtemp->lock);
	sample_temp = simtemp->latest_sample;
	threshold = simtemp->threshold_mc;
	mode = simtemp->mode;
	sampling_ms = simtemp->sampling_ms;
	stats_temp = simtemp->stats;
	mutex_unlock(&simtemp->lock);

	sample_temp.flags = 0; /* Reset flags */

	/* Get current monotonic timestamp BEFORE generating temp */
	sample_temp.timestamp_ns = ktime_get_ns();

	/* --- Temperature Generation Logic --- */
	// Uses simtemp->latest_sample.temp_mc for ramp mode continuity
	switch (mode) {
	case SIMTEMP_MODE_NOISY:
		get_random_bytes(&new_temp, sizeof(new_temp));
		new_temp = 25000 + (new_temp % 5000);
		break;
	case SIMTEMP_MODE_RAMP:
		/* Read the previous temp atomically before calculation */
		new_temp = sample_temp.temp_mc + 100;
		if (new_temp > 100000)
			new_temp = 0;
		break;
	case SIMTEMP_MODE_NORMAL:
	default:
		new_temp = 27500;
		break;
	}

	/* Check threshold */
	if (new_temp > threshold) {
		sample_temp.flags |= SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI;
	}

	if(new_temp < SIMTEMP_THRESHOLD_MC_MIN || new_temp > SIMTEMP_THRESHOLD_MC_MAX) {
		/* Out of bounds error */
		sample_temp.flags |= SIMTEMP_SAMPLE_FLAG_OUT_OF_RANGE;

		pr_warn("simtemp: Generated temperature %d mC out of bounds [%d, %d]\n",
		        new_temp, SIMTEMP_THRESHOLD_MC_MIN, SIMTEMP_THRESHOLD_MC_MAX);
		/* Clamp to valid range */
		if (new_temp < SIMTEMP_THRESHOLD_MC_MIN)
			new_temp = SIMTEMP_THRESHOLD_MC_MIN;
		else
			new_temp = SIMTEMP_THRESHOLD_MC_MAX;
	}

	/*Update countersWS*/

	stats_temp.updates++;
	if (sample_temp.flags & SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI) {
		stats_temp.alerts++;
	}

	if(sample_temp.flags & SIMTEMP_SAMPLE_FLAG_OUT_OF_RANGE) {
		stats_temp.errors++;
	}
	/* --- Update Shared State --- */
	mutex_lock(&simtemp->lock);

	simtemp->stats = stats_temp;
	simtemp->latest_sample = sample_temp;


	/* Mark sample as available */
	simtemp->new_sample_available = true;

	mutex_unlock(&simtemp->lock);
	/* --- End Update Shared State --- */


	/* Wake up any waiting readers */
	wake_up_interruptible(&simtemp->read_wq);
    debug_dbg("Timer: Woke up readers for new sample\n");

	/* Reschedule the timer */
	mod_timer(&simtemp->timer, jiffies + msecs_to_jiffies(sampling_ms));

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
    simtemp->sampling_ms = SIMTEMP_SAMPLING_MS_DEFAULT;
    simtemp->threshold_mc = SIMTEMP_THRESHOLD_MC_DEFAULT;
    simtemp->mode = SIMTEMP_MODE_NORMAL;
    simtemp->latest_sample.temp_mc = SIMTEMP_TEMPERATURE_MC_INITIAL; /* Initial temperature 25 C */
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