/**
 * @file    simtemp_config.h
 * @author  Omar Mendiola
 * @brief   Configuration constants for the NXP simtemp driver.
 * Defines default, minimum, and maximum values for driver parameters.
 * @version 0.1
 * @date    2025-10-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef SIMTEMP_CONFIG_H_
#define SIMTEMP_CONFIG_H_

/* --- Sampling Period Configuration (milliseconds) --- */
#define SIMTEMP_SAMPLING_MS_MIN     100     /* Minimum allowed sampling period (ms) */
#define SIMTEMP_SAMPLING_MS_MAX     60000   /* Maximum allowed sampling period (ms) */
#define SIMTEMP_SAMPLING_MS_DEFAULT 1000    /* Default sampling period (ms) */

/* --- Alert Threshold Configuration (milli-degrees Celsius) --- */
#define SIMTEMP_THRESHOLD_MC_MIN    -50000  /* Minimum allowed threshold (-50.000 C) */
#define SIMTEMP_THRESHOLD_MC_MAX    150000  /* Maximum allowed threshold (150.000 C) */
#define SIMTEMP_THRESHOLD_MC_DEFAULT 50000   /* Default threshold (50.000 C) */

/* Temperature (mC)*/
#define SIMTEMP_TEMPERATURE_MC_INITIAL 25000  /* Initial temperature (25.000 C) */
/* --- Blocking Read Timeout Configuration --- */
/*
 * Use the maximum sampling period as a safety timeout for blocking reads.
 * If no new sample arrives within this time, the read will return an error.
 */
#define SIMTEMP_READ_TIMEOUT_MS     (SIMTEMP_SAMPLING_MS_MAX + 1000) /* Max sampling + 1 sec grace */

#endif /* SIMTEMP_CONFIG_H_ */