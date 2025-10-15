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

    /* --- Temperature Generation Logic --- */
    // TODO: Implement different logic based on simtemp->mode
    switch (simtemp->mode) {
    case SIMTEMP_MODE_NOISY:
        get_random_bytes(&new_temp, sizeof(new_temp));
        new_temp = 25000 + (new_temp % 5000); // Random temp between 25-30 C
        break;
    case SIMTEMP_MODE_RAMP:
        new_temp = simtemp->temp_mc + 100; // Ramp up by 0.1 C
        if (new_temp > 100000) // Reset ramp at 100 C
            new_temp = 0;
        break;
    case SIMTEMP_MODE_NORMAL:
    default:
        new_temp = 27500; // Fixed 27.5 C
        break;
    }

    mutex_lock(&simtemp->lock);

    simtemp->temp_mc = new_temp;
    simtemp->stats.updates++;

    if (simtemp->temp_mc > simtemp->threshold_mc) {
        simtemp->stats.alerts++;
    }

    mutex_unlock(&simtemp->lock);

    /* Reschedule the timer */
    mod_timer(&simtemp->timer, jiffies + msecs_to_jiffies(simtemp->sampling_ms));
}

/**
 * @brief Initializes the simulator.
 *
 * Sets up the initial state and starts the kernel timer.
 *
 * @param dev Pointer to the main simtemp_dev structure.
 * @return int Always returns 0.
 */
int nxp_simtemp_simulator_init(struct simtemp_dev *dev)
{
    mutex_init(&dev->lock);

    /* Set default values */
    dev->sampling_ms = 1000;
    dev->threshold_mc = 50000;
    dev->mode = SIMTEMP_MODE_NORMAL;
    dev->temp_mc = 25000; // Initial temperature 25 C

    timer_setup(&dev->timer, simtemp_timer_callback, 0);
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->sampling_ms));

    return 0;
}

/**
 * @brief Deinitializes the simulator.
 *
 * Stops the kernel timer.
 *
 * @param dev Pointer to the main simtemp_dev structure.
 */
void nxp_simtemp_simulator_exit(struct simtemp_dev *dev)
{
    del_timer_sync(&dev->timer);
}