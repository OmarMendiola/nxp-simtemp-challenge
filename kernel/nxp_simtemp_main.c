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

#include "nxp_simtemp.h"

/* Forward declarations for functions defined in other files */
extern int nxp_simtemp_miscdev_init(struct simtemp_dev *dev);
extern void nxp_simtemp_miscdev_exit(void);
extern int nxp_simtemp_simulator_init(struct simtemp_dev *dev);
extern void nxp_simtemp_simulator_exit(struct simtemp_dev *dev);
extern int nxp_simtemp_sysfs_init(struct device *dev);
extern void nxp_simtemp_sysfs_exit(struct device *dev);


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
    int ret;

    dev_info(dev, "Probing device\n");

    simtemp = devm_kzalloc(dev, sizeof(*simtemp), GFP_KERNEL);
    if (!simtemp) {
        return -ENOMEM;
    }

    simtemp->dev = dev;
    platform_set_drvdata(pdev, simtemp);

    ret = nxp_simtemp_simulator_init(simtemp);
    if (ret) {
        dev_err(dev, "Failed to initialize simulator\n");
        return ret;
    }

    /* 1. Initialize misc device, which now populates simtemp->misc_dev */
    ret = nxp_simtemp_miscdev_init(simtemp);
    if (ret) {
        dev_err(dev, "Failed to initialize misc device\n");
        goto err_simulator;
    }

    /* 2. Associate our main data struct with the new misc device's device struct */
    dev_set_drvdata(simtemp->misc_dev, simtemp);

    /* 3. Initialize sysfs using the misc device's device struct */
    ret = nxp_simtemp_sysfs_init(simtemp->misc_dev);
    if (ret) {
        dev_err(dev, "Failed to initialize sysfs\n");
        goto err_miscdev;
    }

    dev_info(dev, "Device successfully probed\n");
    return 0;

err_miscdev:
    nxp_simtemp_miscdev_exit();
err_simulator:
    nxp_simtemp_simulator_exit(simtemp);

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

    nxp_simtemp_miscdev_exit();
    nxp_simtemp_simulator_exit(simtemp);
    nxp_simtemp_sysfs_exit(&pdev->dev);

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

    pdev_test = platform_device_register_simple("nxp_simtemp", -1, NULL, 0);
    if (IS_ERR(pdev_test)) {
        pr_err("Failed to register test platform device\n");
        return PTR_ERR(pdev_test);
    }

    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        pr_err("Failed to register platform driver\n");
        platform_device_unregister(pdev_test);
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
    platform_driver_unregister(&nxp_simtemp_driver);
    platform_device_unregister(pdev_test);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Omar Mendiola");
MODULE_DESCRIPTION("NXP Simulated Temperature Sensor Driver");

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

