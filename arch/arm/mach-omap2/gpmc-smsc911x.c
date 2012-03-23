/*
 * linux/arch/arm/mach-omap2/gpmc-smsc911x.c
 *
 * Copyright (C) 2009 Li-Pro.Net
 * Stephan Linz <linz@li-pro.net>
 *
 * Modified from linux/arch/arm/mach-omap2/gpmc-smc91x.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/smsc911x.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>

#include <plat/board.h>
#include <plat/gpmc.h>
#include <plat/gpmc-smsc911x.h>

static struct resource gpmc_smsc911x_resources[] = {
	[0] = {
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.flags		= IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL,
	},
};

static struct smsc911x_platform_config gpmc_smsc911x_config = {
	.phy_interface	= PHY_INTERFACE_MODE_MII,
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_OPEN_DRAIN,
};

static struct regulator_consumer_supply gpmc_smsc911x_supply[] = {
	REGULATOR_SUPPLY("vddvario", "smsc911x.0"),
	REGULATOR_SUPPLY("vdd33a", "smsc911x.0"),
};

/* Generic regulator definition to satisfy smsc911x */
static struct regulator_init_data gpmc_smsc911x_reg_init_data = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(gpmc_smsc911x_supply),
	.consumer_supplies	= gpmc_smsc911x_supply,
};

static struct fixed_voltage_config gpmc_smsc911x_fixed_reg_data = {
	.supply_name		= "gpmc_smsc911x",
	.microvolts		= 3300000,
	.gpio			= -EINVAL,
	.startup_delay		= 0,
	.enable_high		= 0,
	.enabled_at_boot	= 1,
	.init_data		= &gpmc_smsc911x_reg_init_data,
};

/*
 * Platform device id of 42 is a temporary fix to avoid conflicts
 * with other reg-fixed-voltage devices. The real fix should
 * involve the driver core providing a way of dynamically
 * assigning a unique id on registration for platform devices
 * in the same name space.
 */
static struct platform_device gpmc_smsc911x_regulator = {
	.name		= "reg-fixed-voltage",
	.id		= 42,
	.dev = {
		.platform_data	= &gpmc_smsc911x_fixed_reg_data,
	},
};

/*
 * Initialize smsc911x device connected to the GPMC. Note that we
 * assume that pin multiplexing is done in the board-*.c file,
 * or in the bootloader.
 */
void __init gpmc_smsc911x_init(struct omap_smsc911x_platform_data *gpmc_cfg)
{
	struct platform_device *pdev;
	unsigned long cs_mem_base;
	int ret;

	if (!gpmc_cfg->id) {
		ret = platform_device_register(&gpmc_smsc911x_regulator);
		if (ret < 0) {
			pr_err("Unable to register smsc911x regulators: %d\n",
			       ret);
			return;
		}
	}

	if (gpmc_cs_request(gpmc_cfg->cs, SZ_16M, &cs_mem_base) < 0) {
		pr_err("Failed to request GPMC mem region\n");
		return;
	}

	gpmc_smsc911x_resources[0].start = cs_mem_base + 0x0;
	gpmc_smsc911x_resources[0].end = cs_mem_base + 0xff;

	if (gpio_request_one(gpmc_cfg->gpio_irq, GPIOF_IN, "smsc911x irq")) {
		pr_err("Failed to request IRQ GPIO%d\n", gpmc_cfg->gpio_irq);
		goto free1;
	}

	gpmc_smsc911x_resources[1].start = gpio_to_irq(gpmc_cfg->gpio_irq);

	if (gpio_is_valid(gpmc_cfg->gpio_reset)) {
		ret = gpio_request_one(gpmc_cfg->gpio_reset,
				       GPIOF_OUT_INIT_HIGH, "smsc911x reset");
		if (ret) {
			pr_err("Failed to request reset GPIO%d\n",
			       gpmc_cfg->gpio_reset);
			goto free2;
		}

		gpio_set_value(gpmc_cfg->gpio_reset, 0);
		msleep(100);
		gpio_set_value(gpmc_cfg->gpio_reset, 1);
	}

	gpmc_smsc911x_config.flags = gpmc_cfg->flags ? : SMSC911X_USE_16BIT;

	pdev = platform_device_register_resndata(NULL, "smsc911x", gpmc_cfg->id,
		 gpmc_smsc911x_resources, ARRAY_SIZE(gpmc_smsc911x_resources),
		 &gpmc_smsc911x_config, sizeof(gpmc_smsc911x_config));
	if (!pdev) {
		pr_err("Unable to register platform device\n");
		gpio_free(gpmc_cfg->gpio_reset);
		goto free2;
	}

	return;

free2:
	gpio_free(gpmc_cfg->gpio_irq);
free1:
	gpmc_cs_free(gpmc_cfg->cs);

	pr_err("Could not initialize smsc911x device\n");
}
