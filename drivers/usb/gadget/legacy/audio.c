/*
 * audio.c -- Audio gadget driver
 *
 * Copyright (C) 2008 Bryan Wu <cooloney@kernel.org>
 * Copyright (C) 2008 Analog Devices, Inc
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

/* #define VERBOSE_DEBUG */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>

#include "gadget_chips.h"
#define DRIVER_DESC		"Linux USB Audio Gadget"
#define DRIVER_VERSION		"Feb 2, 2012"

USB_GADGET_COMPOSITE_OPTIONS();

#ifndef CONFIG_GADGET_UAC1
#include "u_uac2.h"

/* Playback(USB-IN) Default Stereo - Fl/Fr */
static int p_chmask = UAC2_DEF_PCHMASK;
module_param(p_chmask, uint, S_IRUGO);
MODULE_PARM_DESC(p_chmask, "Playback Channel Mask");

/* Playback Default 48 KHz */
static int p_srate = UAC2_DEF_PSRATE;
module_param(p_srate, uint, S_IRUGO);
MODULE_PARM_DESC(p_srate, "Playback Sampling Rate");

/* Playback Default 16bits/sample */
static int p_ssize = UAC2_DEF_PSSIZE;
module_param(p_ssize, uint, S_IRUGO);
MODULE_PARM_DESC(p_ssize, "Playback Sample Size(bytes)");

/* Capture(USB-OUT) Default Stereo - Fl/Fr */
static int c_chmask = UAC2_DEF_CCHMASK;
module_param(c_chmask, uint, S_IRUGO);
MODULE_PARM_DESC(c_chmask, "Capture Channel Mask");

/* Capture Default 64 KHz */
static int c_srate = UAC2_DEF_CSRATE;
module_param(c_srate, uint, S_IRUGO);
MODULE_PARM_DESC(c_srate, "Capture Sampling Rate");

/* Capture Default 16bits/sample */
static int c_ssize = UAC2_DEF_CSSIZE;
module_param(c_ssize, uint, S_IRUGO);
MODULE_PARM_DESC(c_ssize, "Capture Sample Size(bytes)");
#endif

/* string IDs are assigned dynamically */

static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "",
	[USB_GADGET_PRODUCT_IDX].s = DRIVER_DESC,
	[USB_GADGET_SERIAL_IDX].s = "",
	{  } /* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language = 0x0409,	/* en-us */
	.strings = strings_dev,
};

static struct usb_gadget_strings *audio_strings[] = {
	&stringtab_dev,
	NULL,
};

#ifndef CONFIG_GADGET_UAC1
static struct usb_function_instance *fi_uac2;
static struct usb_function *f_uac2;
#endif

#ifdef CONFIG_GADGET_UAC1
#include "u_uac1.h"
#include "u_uac1.c"
#include "f_uac1.c"
#endif

/*-------------------------------------------------------------------------*/

/* DO NOT REUSE THESE IDs with a protocol-incompatible driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */

/* Thanks to Linux Foundation for donating this product ID. */
#define AUDIO_VENDOR_NUM		0x1d6b	/* Linux Foundation */
#define AUDIO_PRODUCT_NUM		0x0101	/* Linux-USB Audio Gadget */

/*-------------------------------------------------------------------------*/

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		__constant_cpu_to_le16(0x200),

#ifdef CONFIG_GADGET_UAC1
	.bDeviceClass =		USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
#else
	.bDeviceClass =		USB_CLASS_MISC,
	.bDeviceSubClass =	0x02,
	.bDeviceProtocol =	0x01,
#endif
	/* .bMaxPacketSize0 = f(hardware) */

	/* Vendor and product id defaults change according to what configs
	 * we support.  (As does bNumConfigurations.)  These values can
	 * also be overridden by module parameters.
	 */
	.idVendor =		__constant_cpu_to_le16(AUDIO_VENDOR_NUM),
	.idProduct =		__constant_cpu_to_le16(AUDIO_PRODUCT_NUM),
	/* .bcdDevice = f(hardware) */
	/* .iManufacturer = DYNAMIC */
	/* .iProduct = DYNAMIC */
	/* NO SERIAL NUMBER */
	.bNumConfigurations =	1,
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,

	/* REVISIT SRP-only hardware is possible, although
	 * it would not be called "OTG" ...
	 */
	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};

/*-------------------------------------------------------------------------*/

static int __init audio_do_config(struct usb_configuration *c)
{
#ifndef CONFIG_GADGET_UAC1
	int status;
#endif

	/* FIXME alloc iConfiguration string, set it in c->strings */

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

#ifdef CONFIG_GADGET_UAC1
	audio_bind_config(c);
#else
	f_uac2 = usb_get_function(fi_uac2);
	if (IS_ERR(f_uac2)) {
		status = PTR_ERR(f_uac2);
		return status;
	}

	status = usb_add_function(c, f_uac2);
	if (status < 0) {
		usb_put_function(f_uac2);
		return status;
	}
#endif

	return 0;
}

static struct usb_configuration audio_config_driver = {
	.label			= DRIVER_DESC,
	.bConfigurationValue	= 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

/*-------------------------------------------------------------------------*/

static int __init audio_bind(struct usb_composite_dev *cdev)
{
#ifndef CONFIG_GADGET_UAC1
	struct f_uac2_opts	*uac2_opts;
#endif
	int			status;

#ifndef CONFIG_GADGET_UAC1
	fi_uac2 = usb_get_function_instance("uac2");
	if (IS_ERR(fi_uac2))
		return PTR_ERR(fi_uac2);
#endif

#ifndef CONFIG_GADGET_UAC1
	uac2_opts = container_of(fi_uac2, struct f_uac2_opts, func_inst);
	uac2_opts->p_chmask = p_chmask;
	uac2_opts->p_srate = p_srate;
	uac2_opts->p_ssize = p_ssize;
	uac2_opts->c_chmask = c_chmask;
	uac2_opts->c_srate = c_srate;
	uac2_opts->c_ssize = c_ssize;
#endif

	status = usb_string_ids_tab(cdev, strings_dev);
	if (status < 0)
		goto fail;
	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;

	status = usb_add_config(cdev, &audio_config_driver, audio_do_config);
	if (status < 0)
		goto fail;
	usb_composite_overwrite_options(cdev, &coverwrite);

	INFO(cdev, "%s, version: %s\n", DRIVER_DESC, DRIVER_VERSION);
	return 0;

fail:
#ifndef CONFIG_GADGET_UAC1
	usb_put_function_instance(fi_uac2);
#endif
	return status;
}

static int __exit audio_unbind(struct usb_composite_dev *cdev)
{
#ifdef CONFIG_GADGET_UAC1
	gaudio_cleanup();
#else
	if (!IS_ERR_OR_NULL(f_uac2))
		usb_put_function(f_uac2);
	if (!IS_ERR_OR_NULL(fi_uac2))
		usb_put_function_instance(fi_uac2);
#endif
	return 0;
}

static __refdata struct usb_composite_driver audio_driver = {
	.name		= "g_audio",
	.dev		= &device_desc,
	.strings	= audio_strings,
	.max_speed	= USB_SPEED_HIGH,
	.bind		= audio_bind,
	.unbind		= __exit_p(audio_unbind),
};

module_usb_composite_driver(audio_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Bryan Wu <cooloney@kernel.org>");
MODULE_LICENSE("GPL");

