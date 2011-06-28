/**
 * udc.c - Core UDC Framework
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Felipe Balbi <balbi@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/err.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

/**
 * struct usb_udc - describes one usb device controller
 * @driver - the gadget driver pointer. For use by the class code
 * @dev - the child device to the actual controller
 * @gadget - the gadget. For use by the class code
 * @list - for use by the udc class driver
 *
 * This represents the internal data structure which is used by the UDC-class
 * to hold information about udc driver and gadget together.
 */
struct usb_udc {
	struct usb_gadget_driver	*driver;
	struct usb_gadget		*gadget;
	struct device			dev;
	struct list_head		list;
};

static struct class *udc_class;
static struct device_type udc_device_type;
static LIST_HEAD(udc_list);
static DEFINE_MUTEX(udc_lock);

/* ------------------------------------------------------------------------- */

/**
 * usb_gadget_start - tells usb device controller to start up
 * @gadget: The gadget we want to get started
 * @driver: The driver we want to bind to @gadget
 * @bind: The bind function for @driver
 *
 * This call is issued by the UDC Class driver when it's about
 * to register a gadget driver to the device controller, before
 * calling gadget driver's bind() method.
 *
 * It allows the controller to be powered off until strictly
 * necessary to have it powered on.
 *
 * Returns zero on success, else negative errno.
 */
static inline int usb_gadget_start(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *))
{
	return gadget->ops->start(driver, bind);
}

/**
 * usb_gadget_stop - tells usb device controller we don't need it anymore
 * @gadget: The device we want to stop activity
 * @driver: The driver to unbind from @gadget
 *
 * This call is issued by the UDC Class driver after calling
 * gadget driver's unbind() method.
 *
 * The details are implementation specific, but it can go as
 * far as powering off UDC completely and disable its data
 * line pullups.
 */
static inline void usb_gadget_stop(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	gadget->ops->stop(driver);
}

/**
 * usb_udc_release - release the usb_udc struct
 * @dev: the dev member within usb_udc
 *
 * This is called by driver's core in order to free memory once the last
 * reference is released.
 */
static void usb_udc_release(struct device *dev)
{
	struct usb_udc *udc;

	udc = container_of(dev, struct usb_udc, dev);
	dev_dbg(dev, "releasing '%s'\n", dev_name(dev));
	kfree(udc);
}

/**
 * usb_add_gadget_udc - adds a new gadget to the udc class driver list
 * @parent: the parent device to this udc. Usually the controller
 * driver's device.
 * @gadget: the gadget to be added to the list
 *
 * Returns zero on success, negative errno otherwise.
 */
int usb_add_gadget_udc(struct device *parent, struct usb_gadget *gadget)
{
	struct usb_udc		*udc;
	int			ret = -ENOMEM;

	udc = kzalloc(sizeof(*udc), GFP_KERNEL);
	if (!udc)
		goto err1;

	device_initialize(&udc->dev);
	udc->dev.release = usb_udc_release;
	udc->dev.class = udc_class;
	udc->dev.parent = parent;
	ret = dev_set_name(&udc->dev, "%s", kobject_name(&parent->kobj));
	if (ret)
		goto err2;

	udc->gadget = gadget;

	mutex_lock(&udc_lock);
	list_add_tail(&udc->list, &udc_list);

	ret = device_add(&udc->dev);
	if (ret)
		goto err3;

	mutex_unlock(&udc_lock);

	return 0;
err3:
	list_del(&udc->list);
	mutex_unlock(&udc_lock);

err2:
	put_device(&udc->dev);

err1:
	return ret;
}
EXPORT_SYMBOL_GPL(usb_add_gadget_udc);

static void usb_gadget_remove_driver(struct usb_udc *udc)
{
	dev_dbg(&udc->dev, "unregistering UDC driver [%s]\n",
			udc->gadget->name);

	kobject_uevent(&udc->dev.kobj, KOBJ_CHANGE);

	usb_gadget_stop(udc->gadget, udc->driver);

	udc->driver = NULL;
	udc->dev.driver = NULL;
}

/**
 * usb_del_gadget_udc - deletes @udc from udc_list
 * @gadget: the gadget to be removed.
 *
 * This, will call usb_gadget_unregister_driver() if
 * the @udc is still busy.
 */
void usb_del_gadget_udc(struct usb_gadget *gadget)
{
	struct usb_udc		*udc = NULL;

	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list)
		if (udc->gadget == gadget)
			goto found;

	dev_err(gadget->dev.parent, "gadget not registered.\n");
	mutex_unlock(&udc_lock);

	return;

found:
	dev_vdbg(gadget->dev.parent, "unregistering gadget\n");

	list_del(&udc->list);
	mutex_unlock(&udc_lock);

	if (udc->driver)
		usb_gadget_remove_driver(udc);

	kobject_uevent(&udc->dev.kobj, KOBJ_REMOVE);
	device_unregister(&udc->dev);
}
EXPORT_SYMBOL_GPL(usb_del_gadget_udc);

/* ------------------------------------------------------------------------- */

int usb_gadget_probe_driver(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *))
{
	struct usb_udc		*udc = NULL;
	int			ret;

	if (!driver || !bind || !driver->setup)
		return -EINVAL;

	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list) {
		/* For now we take the first one */
		if (!udc->driver)
			goto found;
	}

	pr_debug("couldn't find an available UDC\n");
	mutex_unlock(&udc_lock);
	return -ENODEV;

found:
	dev_dbg(&udc->dev, "registering UDC driver [%s]\n",
			driver->function);

	udc->driver = driver;
	udc->dev.driver = &driver->driver;

	ret = usb_gadget_start(udc->gadget, driver, bind);
	if (ret)
		goto err1;

	kobject_uevent(&udc->dev.kobj, KOBJ_CHANGE);
	mutex_unlock(&udc_lock);
	return 0;

err1:
	dev_err(&udc->dev, "failed to start %s: %d\n",
			udc->driver->function, ret);
	udc->driver = NULL;
	udc->dev.driver = NULL;
	mutex_unlock(&udc_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_probe_driver);

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct usb_udc		*udc = NULL;
	int			ret = -ENODEV;

	if (!driver || !driver->unbind)
		return -EINVAL;

	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list)
		if (udc->driver == driver) {
			usb_gadget_remove_driver(udc);
			ret = 0;
			break;
		}

	mutex_unlock(&udc_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_unregister_driver);

/* ------------------------------------------------------------------------- */

static ssize_t usb_udc_srp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_udc		*udc = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1"))
		usb_gadget_wakeup(udc->gadget);

	return n;
}
static DEVICE_ATTR(srp, S_IWUSR, NULL, usb_udc_srp_store);

static ssize_t usb_udc_softconn_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_udc		*udc = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "connect")) {
		usb_gadget_connect(udc->gadget);
	} else if (sysfs_streq(buf, "disconnect")) {
		usb_gadget_disconnect(udc->gadget);
	} else {
		dev_err(dev, "unsupported command '%s'\n", buf);
		return -EINVAL;
	}

	return n;
}
static DEVICE_ATTR(soft_connect, S_IWUSR, NULL, usb_udc_softconn_store);

static ssize_t usb_udc_speed_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usb_udc		*udc = dev_get_drvdata(dev);
	struct usb_gadget	*gadget = udc->gadget;

	switch (gadget->speed) {
	case USB_SPEED_LOW:
		return snprintf(buf, PAGE_SIZE, "low-speed\n");
	case USB_SPEED_FULL:
		return snprintf(buf, PAGE_SIZE, "full-speed\n");
	case USB_SPEED_HIGH:
		return snprintf(buf, PAGE_SIZE, "high-speed\n");
	case USB_SPEED_WIRELESS:
		return snprintf(buf, PAGE_SIZE, "wireless\n");
	case USB_SPEED_SUPER:
		return snprintf(buf, PAGE_SIZE, "super-speed\n");
	case USB_SPEED_UNKNOWN:	/* FALLTHROUGH */
	default:
		return snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
	}
}
static DEVICE_ATTR(speed, S_IRUSR, usb_udc_speed_show, NULL);

#define USB_UDC_ATTR(name)					\
ssize_t usb_udc_##name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)	\
{								\
	struct usb_udc		*udc = dev_get_drvdata(dev);	\
	struct usb_gadget	*gadget = udc->gadget;		\
								\
	return snprintf(buf, PAGE_SIZE, "%d\n", gadget->name);	\
}								\
static DEVICE_ATTR(name, S_IRUSR, usb_udc_##name##_show, NULL)

static USB_UDC_ATTR(is_dualspeed);
static USB_UDC_ATTR(is_otg);
static USB_UDC_ATTR(is_a_peripheral);
static USB_UDC_ATTR(b_hnp_enable);
static USB_UDC_ATTR(a_hnp_support);
static USB_UDC_ATTR(a_alt_hnp_support);

static struct attribute *usb_udc_attrs[] = {
	&dev_attr_srp.attr,
	&dev_attr_soft_connect.attr,
	&dev_attr_speed.attr,

	&dev_attr_is_dualspeed.attr,
	&dev_attr_is_otg.attr,
	&dev_attr_is_a_peripheral.attr,
	&dev_attr_b_hnp_enable.attr,
	&dev_attr_a_hnp_support.attr,
	&dev_attr_a_alt_hnp_support.attr,
	NULL,
};

static const struct attribute_group usb_udc_attr_group = {
	.attrs = usb_udc_attrs,
};

static const struct attribute_group *usb_udc_attr_groups[] = {
	&usb_udc_attr_group,
	NULL,
};

static int usb_udc_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct usb_udc		*udc = container_of(dev, struct usb_udc, dev);
	int			ret;

	ret = add_uevent_var(env, "USB_UDC_NAME=%s", udc->gadget->name);
	if (ret) {
		dev_err(dev, "failed to add uevent USB_UDC_NAME\n");
		return ret;
	}

	if (udc->driver) {
		ret = add_uevent_var(env, "USB_UDC_DRIVER=%s",
				udc->driver->function);
		if (ret) {
			dev_err(dev, "failed to add uevent USB_UDC_DRIVER\n");
			return ret;
		}
	}

	return 0;
}

static int __init usb_udc_init(void)
{
	udc_class = class_create(THIS_MODULE, "udc");
	if (IS_ERR(udc_class)) {
		pr_err("failed to create udc class --> %ld\n",
				PTR_ERR(udc_class));
		return PTR_ERR(udc_class);
	}

	udc_class->dev_uevent = usb_udc_uevent;
	udc_device_type.groups = usb_udc_attr_groups;

	return 0;
}
subsys_initcall(usb_udc_init);

static void __exit usb_udc_exit(void)
{
	class_destroy(udc_class);
}
module_exit(usb_udc_exit);

MODULE_DESCRIPTION("UDC Framework");
MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("GPL v2");
