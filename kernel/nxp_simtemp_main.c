/**
 * @file    nxp_simtemp.c
 * @author  Omar Mendiola
 * @brief   Main file for the NXP simulated temperature sensor driver.
 * This file handles the platform driver registration, probe/remove
 * logic, and module initialization/exit.
 * @version 0.1
 * @date    2025-10-14
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/property.h>

#include "nxp_simtemp.h"

/* Forward declarations for functions defined in other files */
extern int nxp_simtemp_miscdev_init(struct simtemp_dev *simtemp);
extern void nxp_simtemp_miscdev_exit(struct simtemp_dev *simtemp);
extern int nxp_simtemp_simulator_init(struct simtemp_dev *simtemp);
extern void nxp_simtemp_simulator_exit(struct simtemp_dev *simtemp);
extern int nxp_simtemp_sysfs_init(struct simtemp_dev *simtemp);
extern void nxp_simtemp_sysfs_exit(struct simtemp_dev *simtemp);
extern void nxp_simtemp_locks_init(struct simtemp_dev *simtemp);
extern void nxp_simtemp_locks_exit(struct simtemp_dev *simtemp);

/**
 * @brief Reads configuration properties from the Device Tree node.
 *
 * Reads "sampling-ms" and "threshold-mC". If properties are not found
 * or invalid, default values from simtemp_config.h are used.
 *
 * @param dev Pointer to the device structure.
 * @param simtemp Pointer to the driver's instance data structure.
 */
static void nxp_simtemp_read_dt_config(struct device *dev, struct simtemp_dev *simtemp)
{
	int ret;
	u32 val_u32;

	/* Read sampling-ms */
	ret = device_property_read_u32(dev, "sampling-ms", &val_u32);
	if (ret) {
		dev_info(dev, "DT: 'sampling-ms' not found, using default %u ms\n",
		         SIMTEMP_SAMPLING_MS_DEFAULT);
		simtemp->sampling_ms = SIMTEMP_SAMPLING_MS_DEFAULT;
	} else {
		/* Validate the value read from DT */
		if (val_u32 < SIMTEMP_SAMPLING_MS_MIN || val_u32 > SIMTEMP_SAMPLING_MS_MAX) {
			dev_warn(dev, "DT: 'sampling-ms' value %u out of range [%u-%u], using default %u ms\n",
			         val_u32, SIMTEMP_SAMPLING_MS_MIN, SIMTEMP_SAMPLING_MS_MAX, SIMTEMP_SAMPLING_MS_DEFAULT);
			simtemp->sampling_ms = SIMTEMP_SAMPLING_MS_DEFAULT;
		} else {
			simtemp->sampling_ms = val_u32;
			dev_info(dev, "DT: 'sampling-ms' set to %u ms\n", simtemp->sampling_ms);
		}
	}

	/* Read threshold-mC (signed value) */
	ret = device_property_read_u32(dev, "threshold-mC", &val_u32);
	if (ret) {
		dev_info(dev, "DT: 'threshold-mC' not found, using default %d mC\n",
		         SIMTEMP_THRESHOLD_MC_DEFAULT);
		simtemp->threshold_mc = SIMTEMP_THRESHOLD_MC_DEFAULT;
	} else {
		/* Validate the value read from DT */
		if (val_u32 < SIMTEMP_THRESHOLD_MC_MIN || val_u32 > SIMTEMP_THRESHOLD_MC_MAX) {
			dev_warn(dev, "DT: 'threshold-mC' value %d out of range [%d-%d], using default %d mC\n",
			         val_u32, SIMTEMP_THRESHOLD_MC_MIN, SIMTEMP_THRESHOLD_MC_MAX, SIMTEMP_THRESHOLD_MC_DEFAULT);
			simtemp->threshold_mc = SIMTEMP_THRESHOLD_MC_DEFAULT;
		} else {
			simtemp->threshold_mc = val_u32;
			dev_info(dev, "DT: 'threshold-mC' set to %d mC\n", simtemp->threshold_mc);
		}
	}
    /* Add reads for other properties like 'mode' if needed */
}

/**
 * @brief Probe function for the platform driver.
 *
 * This function is called by the kernel when a device matching the driver's
 * ID table is found. It initializes the device structure, sysfs attributes,
 * the simulator, and the misc device interface.
 *
 * @param pdev Pointer to the platform_device structure.
 * @return int 0 on success, or a negative error code on failure.
 */
static int nxp_simtemp_probe(struct platform_device *pdev)
{
struct device *dev = &pdev->dev;
    struct simtemp_dev *simtemp;
    int ret = 0;

     dev_info(dev, "Probing device\n");

    simtemp = devm_kzalloc(dev, sizeof(*simtemp), GFP_KERNEL);
    if (!simtemp) {
        return -ENOMEM;
    }

    debug_pr_addr("simtemp_dev",simtemp);
    debug_pr_addr("misc_dev", &simtemp->misc_dev);

    simtemp->dev = dev;
    platform_set_drvdata(pdev, simtemp);

    /* Read Device Tree configuration FIRST */
	nxp_simtemp_read_dt_config(dev, simtemp);

    /*Initialize simtemp locks (mutex)*/
    nxp_simtemp_locks_init(simtemp);

    /*Initialize misc device, which now populates simtemp->misc_dev */
    ret = nxp_simtemp_miscdev_init(simtemp);
    if (ret) {
        dev_err(dev, "Failed to initialize misc device\n");
        goto err_cleanup;
    }

    /* Initialize sysfs using the misc device's device struct */
    ret = nxp_simtemp_sysfs_init(simtemp);
    if (ret) {
        dev_err(dev, "Failed to initialize sysfs\n");
        goto err_miscdev;
    }

    /* Initialize the temperature simulator (timer)*/
   ret = nxp_simtemp_simulator_init(simtemp);
    if (ret) {
        dev_err(dev, "Failed to initialize simulator\n");
        goto err_sysfs;
    }

    dev_info(dev, "Device successfully probed\n");
    return 0;

    /*Action error section*/
//err_simulator: Not necessary for now
//    nxp_simtemp_simulator_exit(simtemp);
err_sysfs:
    nxp_simtemp_sysfs_exit(simtemp);
err_miscdev:
    nxp_simtemp_miscdev_exit(simtemp);
err_cleanup:
    nxp_simtemp_locks_exit(simtemp);

    return ret;
}

/**
 * @brief Remove function for the platform driver.
 *
 * This function is called by the kernel when the device is being removed.
 * It cleans up all resources allocated during the probe function.
 *
 * @param pdev Pointer to the platform_device structure.
 * @return int Always returns 0.
 */
static int nxp_simtemp_remove(struct platform_device *pdev)
{
 struct simtemp_dev *simtemp = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "Removing device\n");

    // /* Clean up in reverse order of creation */
    debug_pr_delay("Removing Simulator\n");
    nxp_simtemp_simulator_exit(simtemp);

    debug_pr_delay("Removing Sysfs\n");
    nxp_simtemp_sysfs_exit(simtemp);
    
    debug_pr_delay("Removing Miscdev\n");
    nxp_simtemp_miscdev_exit(simtemp);

    //mutex is remove by devm_kzalloc automaticlly
    debug_pr_delay("Removing Locks\n");

    dev_info(&pdev->dev, "Removing device Done\n");
    return 0;
}

static const struct of_device_id nxp_simtemp_of_match[] = {
    { .compatible = "nxp,simtemp", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_of_match);

static struct platform_driver nxp_simtemp_driver = {
    .driver = {
        .name = "nxp_simtemp",
        .of_match_table = nxp_simtemp_of_match,
    },
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
};

static struct platform_device *pdev_test;

/**
 * @brief Module initialization function.
 *
 * Registers the platform driver and creates a test platform device.
 *
 * @return int 0 on success, or a negative error code on failure.
 */
static int __init nxp_simtemp_init(void)
{
    int ret;
    pr_info("Initializing NXP simtemp driver\n");

    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        pr_err("Failed to register platform driver\n");
        return ret;
    }

    pdev_test = platform_device_register_simple("nxp_simtemp", -1, NULL, 0);
    if (IS_ERR(pdev_test)) {
        pr_err("Failed to register test platform device\n");
        platform_driver_unregister(&nxp_simtemp_driver); // Limpieza correcta
        return PTR_ERR(pdev_test);
    }


    return ret;
}

/**
 * @brief Module exit function.
 *
 * Unregisters the platform driver and the test platform device.
 */
static void __exit nxp_simtemp_exit(void)
{
    pr_info("Exiting NXP simtemp driver\n");
    debug_pr_delay("device unregister\n");
    platform_device_unregister(pdev_test);
    debug_pr_delay("Driver unregister\n");
    platform_driver_unregister(&nxp_simtemp_driver);
    debug_pr_delay("Exit Done\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Omar Mendiola");
MODULE_DESCRIPTION("NXP Simulated Temperature Sensor Driver");

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

