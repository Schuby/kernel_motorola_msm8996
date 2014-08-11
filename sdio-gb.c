/*
 * SD/MMC Greybus driver.
 *
 * Copyright 2014 Google Inc.
 *
 * Released under the GPLv2 only.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mmc/host.h>
#include "greybus.h"

struct sd_gb_host {
	struct mmc_host	*mmc;
	struct mmc_request *mrq;
	// FIXME - some lock?
};

static const struct greybus_device_id id_table[] = {
	{ GREYBUS_DEVICE(0x43, 0x43) },	/* make shit up */
	{ },	/* terminating NULL entry */
};

static void gb_sd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	// FIXME - do something here...
}

static void gb_sd_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	// FIXME - do something here...
}

static int gb_sd_get_ro(struct mmc_host *mmc)
{
	// FIXME - do something here...
	return 0;
}

static const struct mmc_host_ops gb_sd_ops = {
	.request	= gb_sd_request,
	.set_ios	= gb_sd_set_ios,
	.get_ro		= gb_sd_get_ro,
};

static int sd_gb_probe(struct greybus_device *gdev, const struct greybus_device_id *id)
{
	struct mmc_host *mmc;
	struct sd_gb_host *host;

	mmc = mmc_alloc_host(sizeof(struct sd_gb_host), &gdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	host->mmc = mmc;

	mmc->ops = &gb_sd_ops;
	// FIXME - set up size limits we can handle.

	greybus_set_drvdata(gdev, host);
	return 0;
}

static void sd_gb_disconnect(struct greybus_device *gdev)
{
	struct mmc_host *mmc;
	struct sd_gb_host *host;

	host = greybus_get_drvdata(gdev);
	mmc = host->mmc;

	mmc_remove_host(mmc);
	mmc_free_host(mmc);
}

static struct greybus_driver sd_gb_driver = {
	.probe =	sd_gb_probe,
	.disconnect =	sd_gb_disconnect,
	.id_table =	id_table,
};

module_greybus_driver(sd_gb_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Greybus SD/MMC Host driver");
MODULE_AUTHOR("Greg Kroah-Hartman <gregkh@linuxfoundation.org>");
