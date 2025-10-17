/*
 * @file      nxp_simtemp_locks.c
 * @author    Omar Mendiola
 * @brief     Locks implementation for the NXP simtemp driver.
 * @version   0.1
 * @date      2025-10-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

/* Includes ------------------------------------------------------------------*/
#include "nxp_simtemp.h"

/* Macros ------------------------------------------------------------------*/

/* Constant ------------------------------------------------------------------*/

/* Variables ------------------------------------------------------------------*/

/* Local Function prototypes -----------------------------------------------*/

/* Functions -----------------------------------------------------------------*/
void nxp_simtemp_locks_init(struct simtemp_dev *simtemp)
{
    mutex_init(&simtemp->lock);
}

void nxp_simtemp_locks_exit(struct simtemp_dev *simtemp)
{
    mutex_destroy(&simtemp->lock);
}
