/**
 * @file    simtemp_debug.h
 * @author  Omar Mendiola
 * @brief   Debugging helper macros for the simtemp driver.
 * @version 0.2
 * @date    2025-10-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef SIMTEMP_DEBUG_H_
#define SIMTEMP_DEBUG_H_

/* Includes needed for the macros */
#include <linux/printk.h>
#include <linux/delay.h>

/* --- Debug Control --- */
#define SIMTEMP_DEBUG_ENABLED 1
#define SIMTEMP_DEBUG_DISABLED 0
#define SIMTEMP_DEBUG_DELAY_MS 50

/* Set to 1 to enable debug messages and delays, 0 to disable */
#define SIMTEMP_DEBUG SIMTEMP_DEBUG_ENABLED

/* --- Debug Macros --- */

/**
 * @brief Prints an emergency message and adds a delay if SIMTEMP_DEBUG is enabled.
 * Useful for ensuring a message gets flushed before a potential crash.
 * @param fmt Format string.
 * @param ... Variable arguments for the format string.
 */
#define debug_pr_delay(fmt, ...) \
	do { \
		if (SIMTEMP_DEBUG == SIMTEMP_DEBUG_ENABLED) { \
			pr_emerg("SIMTEMP_DBG: " fmt, ##__VA_ARGS__); \
			msleep(SIMTEMP_DEBUG_DELAY_MS); \
		} \
	} while (0)

/**
 * @brief Prints a debug message with the address of a pointer if SIMTEMP_DEBUG is enabled.
 * @param msg Descriptive message string.
 * @param ptr Pointer whose address is to be printed.
 */
#define debug_pr_addr(msg, ptr) \
	do { \
		if (SIMTEMP_DEBUG == SIMTEMP_DEBUG_ENABLED) { \
			pr_info("SIMTEMP_DBG: %s at %p\n", (msg), (ptr)); \
		} \
	} while (0)

/**
 * @brief Standard debug message print macro.
 * Only prints if SIMTEMP_DEBUG is enabled.
 * @param fmt Format string.
 * @param ... Variable arguments for the format string.
 */
#define debug_dbg(fmt, ...) \
    do { \
        if (SIMTEMP_DEBUG == SIMTEMP_DEBUG_ENABLED) { \
            pr_info("SIMTEMP_DBG: " fmt, ##__VA_ARGS__); \
        } \
    } while (0)


#endif /* SIMTEMP_DEBUG_H_ */