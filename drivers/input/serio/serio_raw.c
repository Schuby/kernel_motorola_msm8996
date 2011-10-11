/*
 * Raw serio device providing access to a raw byte stream from underlying
 * serio port. Closely emulates behavior of pre-2.6 /dev/psaux device
 *
 * Copyright (c) 2004 Dmitry Torokhov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/mutex.h>

#define DRIVER_DESC	"Raw serio driver"

MODULE_AUTHOR("Dmitry Torokhov <dtor@mail.ru>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define SERIO_RAW_QUEUE_LEN	64
struct serio_raw {
	unsigned char queue[SERIO_RAW_QUEUE_LEN];
	unsigned int tail, head;

	char name[16];
	struct kref kref;
	struct serio *serio;
	struct miscdevice dev;
	wait_queue_head_t wait;
	struct list_head client_list;
	struct list_head node;
	bool dead;
};

struct serio_raw_client {
	struct fasync_struct *fasync;
	struct serio_raw *serio_raw;
	struct list_head node;
};

static DEFINE_MUTEX(serio_raw_mutex);
static LIST_HEAD(serio_raw_list);
static unsigned int serio_raw_no;

/*********************************************************************
 *             Interface with userspace (file operations)            *
 *********************************************************************/

static int serio_raw_fasync(int fd, struct file *file, int on)
{
	struct serio_raw_client *client = file->private_data;

	return fasync_helper(fd, file, on, &client->fasync);
}

static struct serio_raw *serio_raw_locate(int minor)
{
	struct serio_raw *serio_raw;

	list_for_each_entry(serio_raw, &serio_raw_list, node) {
		if (serio_raw->dev.minor == minor)
			return serio_raw;
	}

	return NULL;
}

static int serio_raw_open(struct inode *inode, struct file *file)
{
	struct serio_raw *serio_raw;
	struct serio_raw_client *client;
	int retval;

	retval = mutex_lock_interruptible(&serio_raw_mutex);
	if (retval)
		return retval;

	serio_raw = serio_raw_locate(iminor(inode));
	if (!serio_raw) {
		retval = -ENODEV;
		goto out;
	}

	if (serio_raw->dead) {
		retval = -ENODEV;
		goto out;
	}

	client = kzalloc(sizeof(struct serio_raw_client), GFP_KERNEL);
	if (!client) {
		retval = -ENOMEM;
		goto out;
	}

	client->serio_raw = serio_raw;
	file->private_data = client;

	kref_get(&serio_raw->kref);

	serio_pause_rx(serio_raw->serio);
	list_add_tail(&client->node, &serio_raw->client_list);
	serio_continue_rx(serio_raw->serio);

out:
	mutex_unlock(&serio_raw_mutex);
	return retval;
}

static void serio_raw_cleanup(struct kref *kref)
{
	struct serio_raw *serio_raw =
			container_of(kref, struct serio_raw, kref);

	misc_deregister(&serio_raw->dev);
	list_del_init(&serio_raw->node);

	put_device(&serio_raw->serio->dev);
	kfree(serio_raw);
}

static int serio_raw_release(struct inode *inode, struct file *file)
{
	struct serio_raw_client *client = file->private_data;
	struct serio_raw *serio_raw = client->serio_raw;

	mutex_lock(&serio_raw_mutex);

	kref_put(&serio_raw->kref, serio_raw_cleanup);

	mutex_unlock(&serio_raw_mutex);
	return 0;
}

static bool serio_raw_fetch_byte(struct serio_raw *serio_raw, char *c)
{
	bool empty;

	serio_pause_rx(serio_raw->serio);

	empty = serio_raw->head == serio_raw->tail;
	if (!empty) {
		*c = serio_raw->queue[serio_raw->tail];
		serio_raw->tail = (serio_raw->tail + 1) % SERIO_RAW_QUEUE_LEN;
	}

	serio_continue_rx(serio_raw->serio);

	return !empty;
}

static ssize_t serio_raw_read(struct file *file, char __user *buffer,
			      size_t count, loff_t *ppos)
{
	struct serio_raw_client *client = file->private_data;
	struct serio_raw *serio_raw = client->serio_raw;
	char uninitialized_var(c);
	ssize_t retval = 0;

	if (serio_raw->dead)
		return -ENODEV;

	if (serio_raw->head == serio_raw->tail && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	retval = wait_event_interruptible(serio_raw->wait,
			serio_raw->head != serio_raw->tail || serio_raw->dead);
	if (retval)
		return retval;

	if (serio_raw->dead)
		return -ENODEV;

	while (retval < count && serio_raw_fetch_byte(serio_raw, &c)) {
		if (put_user(c, buffer++))
			return -EFAULT;
		retval++;
	}

	return retval;
}

static ssize_t serio_raw_write(struct file *file, const char __user *buffer,
			       size_t count, loff_t *ppos)
{
	struct serio_raw_client *client = file->private_data;
	struct serio_raw *serio_raw = client->serio_raw;
	ssize_t written = 0;
	int retval;
	unsigned char c;

	retval = mutex_lock_interruptible(&serio_raw_mutex);
	if (retval)
		return retval;

	if (serio_raw->dead) {
		retval = -ENODEV;
		goto out;
	}

	if (count > 32)
		count = 32;

	while (count--) {
		if (get_user(c, buffer++)) {
			retval = -EFAULT;
			goto out;
		}
		if (serio_write(serio_raw->serio, c)) {
			retval = -EIO;
			goto out;
		}
		written++;
	};

out:
	mutex_unlock(&serio_raw_mutex);
	return written;
}

static unsigned int serio_raw_poll(struct file *file, poll_table *wait)
{
	struct serio_raw_client *client = file->private_data;
	struct serio_raw *serio_raw = client->serio_raw;

	poll_wait(file, &serio_raw->wait, wait);

	if (serio_raw->head != serio_raw->tail)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations serio_raw_fops = {
	.owner		= THIS_MODULE,
	.open		= serio_raw_open,
	.release	= serio_raw_release,
	.read		= serio_raw_read,
	.write		= serio_raw_write,
	.poll		= serio_raw_poll,
	.fasync		= serio_raw_fasync,
	.llseek		= noop_llseek,
};


/*********************************************************************
 *                   Interface with serio port                       *
 *********************************************************************/

static irqreturn_t serio_raw_interrupt(struct serio *serio, unsigned char data,
					unsigned int dfl)
{
	struct serio_raw *serio_raw = serio_get_drvdata(serio);
	struct serio_raw_client *client;
	unsigned int head = serio_raw->head;

	/* we are holding serio->lock here so we are protected */
	serio_raw->queue[head] = data;
	head = (head + 1) % SERIO_RAW_QUEUE_LEN;
	if (likely(head != serio_raw->tail)) {
		serio_raw->head = head;
		list_for_each_entry(client, &serio_raw->client_list, node)
			kill_fasync(&client->fasync, SIGIO, POLL_IN);
		wake_up_interruptible(&serio_raw->wait);
	}

	return IRQ_HANDLED;
}

static int serio_raw_connect(struct serio *serio, struct serio_driver *drv)
{
	struct serio_raw *serio_raw;
	int err;

	serio_raw = kzalloc(sizeof(struct serio_raw), GFP_KERNEL);
	if (!serio_raw) {
		dev_dbg(&serio->dev, "can't allocate memory for a device\n");
		return -ENOMEM;
	}

	mutex_lock(&serio_raw_mutex);

	snprintf(serio_raw->name, sizeof(serio_raw->name),
		 "serio_raw%d", serio_raw_no++);
	kref_init(&serio_raw->kref);
	INIT_LIST_HEAD(&serio_raw->client_list);
	init_waitqueue_head(&serio_raw->wait);

	serio_raw->serio = serio;
	get_device(&serio->dev);

	serio_set_drvdata(serio, serio_raw);

	err = serio_open(serio, drv);
	if (err)
		goto out_free;

	list_add_tail(&serio_raw->node, &serio_raw_list);

	serio_raw->dev.minor = PSMOUSE_MINOR;
	serio_raw->dev.name = serio_raw->name;
	serio_raw->dev.parent = &serio->dev;
	serio_raw->dev.fops = &serio_raw_fops;

	err = misc_register(&serio_raw->dev);
	if (err) {
		serio_raw->dev.minor = MISC_DYNAMIC_MINOR;
		err = misc_register(&serio_raw->dev);
	}

	if (err) {
		dev_err(&serio->dev,
			"failed to register raw access device for %s\n",
			serio->phys);
		goto out_close;
	}

	dev_info(&serio->dev, "raw access enabled on %s (%s, minor %d)\n",
		 serio->phys, serio_raw->name, serio_raw->dev.minor);
	goto out;

out_close:
	serio_close(serio);
	list_del_init(&serio_raw->node);
out_free:
	serio_set_drvdata(serio, NULL);
	put_device(&serio->dev);
	kfree(serio_raw);
out:
	mutex_unlock(&serio_raw_mutex);
	return err;
}

static int serio_raw_reconnect(struct serio *serio)
{
	struct serio_raw *serio_raw = serio_get_drvdata(serio);
	struct serio_driver *drv = serio->drv;

	if (!drv || !serio_raw) {
		dev_dbg(&serio->dev,
			"reconnect request, but serio is disconnected, ignoring...\n");
		return -1;
	}

	/*
	 * Nothing needs to be done here, we just need this method to
	 * keep the same device.
	 */
	return 0;
}

static void serio_raw_disconnect(struct serio *serio)
{
	struct serio_raw *serio_raw;

	mutex_lock(&serio_raw_mutex);

	serio_raw = serio_get_drvdata(serio);

	serio_close(serio);
	serio_set_drvdata(serio, NULL);

	serio_raw->dead = true;
	wake_up_interruptible(&serio_raw->wait);
	kref_put(&serio_raw->kref, serio_raw_cleanup);

	mutex_unlock(&serio_raw_mutex);
}

static struct serio_device_id serio_raw_serio_ids[] = {
	{
		.type	= SERIO_8042,
		.proto	= SERIO_ANY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{
		.type	= SERIO_8042_XL,
		.proto	= SERIO_ANY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(serio, serio_raw_serio_ids);

static struct serio_driver serio_raw_drv = {
	.driver		= {
		.name	= "serio_raw",
	},
	.description	= DRIVER_DESC,
	.id_table	= serio_raw_serio_ids,
	.interrupt	= serio_raw_interrupt,
	.connect	= serio_raw_connect,
	.reconnect	= serio_raw_reconnect,
	.disconnect	= serio_raw_disconnect,
	.manual_bind	= true,
};

static int __init serio_raw_init(void)
{
	return serio_register_driver(&serio_raw_drv);
}

static void __exit serio_raw_exit(void)
{
	serio_unregister_driver(&serio_raw_drv);
}

module_init(serio_raw_init);
module_exit(serio_raw_exit);
