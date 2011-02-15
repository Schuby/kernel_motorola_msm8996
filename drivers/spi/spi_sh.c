/*
 * SH SPI bus driver
 *
 * Copyright (C) 2011  Renesas Solutions Corp.
 *
 * Based on pxa2xx_spi.c:
 * Copyright (C) 2005 Stephen Street / StreetFire Sound Labs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spi/spi.h>

#define SPI_SH_TBR		0x00
#define SPI_SH_RBR		0x00
#define SPI_SH_CR1		0x08
#define SPI_SH_CR2		0x10
#define SPI_SH_CR3		0x18
#define SPI_SH_CR4		0x20
#define SPI_SH_CR5		0x28

/* CR1 */
#define SPI_SH_TBE		0x80
#define SPI_SH_TBF		0x40
#define SPI_SH_RBE		0x20
#define SPI_SH_RBF		0x10
#define SPI_SH_PFONRD		0x08
#define SPI_SH_SSDB		0x04
#define SPI_SH_SSD		0x02
#define SPI_SH_SSA		0x01

/* CR2 */
#define SPI_SH_RSTF		0x80
#define SPI_SH_LOOPBK		0x40
#define SPI_SH_CPOL		0x20
#define SPI_SH_CPHA		0x10
#define SPI_SH_L1M0		0x08

/* CR3 */
#define SPI_SH_MAX_BYTE		0xFF

/* CR4 */
#define SPI_SH_TBEI		0x80
#define SPI_SH_TBFI		0x40
#define SPI_SH_RBEI		0x20
#define SPI_SH_RBFI		0x10
#define SPI_SH_WPABRT		0x04
#define SPI_SH_SSS		0x01

/* CR8 */
#define SPI_SH_P1L0		0x80
#define SPI_SH_PP1L0		0x40
#define SPI_SH_MUXI		0x20
#define SPI_SH_MUXIRQ		0x10

#define SPI_SH_FIFO_SIZE	32
#define SPI_SH_SEND_TIMEOUT	(3 * HZ)
#define SPI_SH_RECEIVE_TIMEOUT	(HZ >> 3)

#undef DEBUG

struct spi_sh_data {
	void __iomem *addr;
	int irq;
	struct spi_master *master;
	struct list_head queue;
	struct workqueue_struct *workqueue;
	struct work_struct ws;
	unsigned long cr1;
	wait_queue_head_t wait;
	spinlock_t lock;
};

static void spi_sh_write(struct spi_sh_data *ss, unsigned long data,
			     unsigned long offset)
{
	writel(data, ss->addr + offset);
}

static unsigned long spi_sh_read(struct spi_sh_data *ss, unsigned long offset)
{
	return readl(ss->addr + offset);
}

static void spi_sh_set_bit(struct spi_sh_data *ss, unsigned long val,
				unsigned long offset)
{
	unsigned long tmp;

	tmp = spi_sh_read(ss, offset);
	tmp |= val;
	spi_sh_write(ss, tmp, offset);
}

static void spi_sh_clear_bit(struct spi_sh_data *ss, unsigned long val,
				unsigned long offset)
{
	unsigned long tmp;

	tmp = spi_sh_read(ss, offset);
	tmp &= ~val;
	spi_sh_write(ss, tmp, offset);
}

static void clear_fifo(struct spi_sh_data *ss)
{
	spi_sh_set_bit(ss, SPI_SH_RSTF, SPI_SH_CR2);
	spi_sh_clear_bit(ss, SPI_SH_RSTF, SPI_SH_CR2);
}

static int spi_sh_wait_receive_buffer(struct spi_sh_data *ss)
{
	int timeout = 100000;

	while (spi_sh_read(ss, SPI_SH_CR1) & SPI_SH_RBE) {
		udelay(10);
		if (timeout-- < 0)
			return -ETIMEDOUT;
	}
	return 0;
}

static int spi_sh_wait_write_buffer_empty(struct spi_sh_data *ss)
{
	int timeout = 100000;

	while (!(spi_sh_read(ss, SPI_SH_CR1) & SPI_SH_TBE)) {
		udelay(10);
		if (timeout-- < 0)
			return -ETIMEDOUT;
	}
	return 0;
}

static int spi_sh_send(struct spi_sh_data *ss, struct spi_message *mesg,
			struct spi_transfer *t)
{
	int i, retval = 0;
	int remain = t->len;
	int cur_len;
	unsigned char *data;
	unsigned long tmp;
	long ret;

	if (t->len)
		spi_sh_set_bit(ss, SPI_SH_SSA, SPI_SH_CR1);

	data = (unsigned char *)t->tx_buf;
	while (remain > 0) {
		cur_len = min(SPI_SH_FIFO_SIZE, remain);
		for (i = 0; i < cur_len &&
				!(spi_sh_read(ss, SPI_SH_CR4) &
							SPI_SH_WPABRT) &&
				!(spi_sh_read(ss, SPI_SH_CR1) & SPI_SH_TBF);
				i++)
			spi_sh_write(ss, (unsigned long)data[i], SPI_SH_TBR);

		if (spi_sh_read(ss, SPI_SH_CR4) & SPI_SH_WPABRT) {
			/* Abort SPI operation */
			spi_sh_set_bit(ss, SPI_SH_WPABRT, SPI_SH_CR4);
			retval = -EIO;
			break;
		}

		cur_len = i;

		remain -= cur_len;
		data += cur_len;

		if (remain > 0) {
			ss->cr1 &= ~SPI_SH_TBE;
			spi_sh_set_bit(ss, SPI_SH_TBE, SPI_SH_CR4);
			ret = wait_event_interruptible_timeout(ss->wait,
						 ss->cr1 & SPI_SH_TBE,
						 SPI_SH_SEND_TIMEOUT);
			if (ret == 0 && !(ss->cr1 & SPI_SH_TBE)) {
				printk(KERN_ERR "%s: timeout\n", __func__);
				return -ETIMEDOUT;
			}
		}
	}

	if (list_is_last(&t->transfer_list, &mesg->transfers)) {
		tmp = spi_sh_read(ss, SPI_SH_CR1);
		tmp = tmp & ~(SPI_SH_SSD | SPI_SH_SSDB);
		spi_sh_write(ss, tmp, SPI_SH_CR1);
		spi_sh_set_bit(ss, SPI_SH_SSA, SPI_SH_CR1);

		ss->cr1 &= ~SPI_SH_TBE;
		spi_sh_set_bit(ss, SPI_SH_TBE, SPI_SH_CR4);
		ret = wait_event_interruptible_timeout(ss->wait,
					 ss->cr1 & SPI_SH_TBE,
					 SPI_SH_SEND_TIMEOUT);
		if (ret == 0 && (ss->cr1 & SPI_SH_TBE)) {
			printk(KERN_ERR "%s: timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}

	return retval;
}

static int spi_sh_receive(struct spi_sh_data *ss, struct spi_message *mesg,
			  struct spi_transfer *t)
{
	int i;
	int remain = t->len;
	int cur_len;
	unsigned char *data;
	unsigned long tmp;
	long ret;

	if (t->len > SPI_SH_MAX_BYTE)
		spi_sh_write(ss, SPI_SH_MAX_BYTE, SPI_SH_CR3);
	else
		spi_sh_write(ss, t->len, SPI_SH_CR3);

	tmp = spi_sh_read(ss, SPI_SH_CR1);
	tmp = tmp & ~(SPI_SH_SSD | SPI_SH_SSDB);
	spi_sh_write(ss, tmp, SPI_SH_CR1);
	spi_sh_set_bit(ss, SPI_SH_SSA, SPI_SH_CR1);

	spi_sh_wait_write_buffer_empty(ss);

	data = (unsigned char *)t->rx_buf;
	while (remain > 0) {
		if (remain >= SPI_SH_FIFO_SIZE) {
			ss->cr1 &= ~SPI_SH_RBF;
			spi_sh_set_bit(ss, SPI_SH_RBF, SPI_SH_CR4);
			ret = wait_event_interruptible_timeout(ss->wait,
						 ss->cr1 & SPI_SH_RBF,
						 SPI_SH_RECEIVE_TIMEOUT);
			if (ret == 0 &&
			    spi_sh_read(ss, SPI_SH_CR1) & SPI_SH_RBE) {
				printk(KERN_ERR "%s: timeout\n", __func__);
				return -ETIMEDOUT;
			}
		}

		cur_len = min(SPI_SH_FIFO_SIZE, remain);
		for (i = 0; i < cur_len; i++) {
			if (spi_sh_wait_receive_buffer(ss))
				break;
			data[i] = (unsigned char)spi_sh_read(ss, SPI_SH_RBR);
		}

		remain -= cur_len;
		data += cur_len;
	}

	/* deassert CS when SPI is receiving. */
	if (t->len > SPI_SH_MAX_BYTE) {
		clear_fifo(ss);
		spi_sh_write(ss, 1, SPI_SH_CR3);
	} else {
		spi_sh_write(ss, 0, SPI_SH_CR3);
	}

	return 0;
}

static void spi_sh_work(struct work_struct *work)
{
	struct spi_sh_data *ss = container_of(work, struct spi_sh_data, ws);
	struct spi_message *mesg;
	struct spi_transfer *t;
	unsigned long flags;
	int ret;

	pr_debug("%s: enter\n", __func__);

	spin_lock_irqsave(&ss->lock, flags);
	while (!list_empty(&ss->queue)) {
		mesg = list_entry(ss->queue.next, struct spi_message, queue);
		list_del_init(&mesg->queue);

		spin_unlock_irqrestore(&ss->lock, flags);
		list_for_each_entry(t, &mesg->transfers, transfer_list) {
			pr_debug("tx_buf = %p, rx_buf = %p\n",
					t->tx_buf, t->rx_buf);
			pr_debug("len = %d, delay_usecs = %d\n",
					t->len, t->delay_usecs);

			if (t->tx_buf) {
				ret = spi_sh_send(ss, mesg, t);
				if (ret < 0)
					goto error;
			}
			if (t->rx_buf) {
				ret = spi_sh_receive(ss, mesg, t);
				if (ret < 0)
					goto error;
			}
			mesg->actual_length += t->len;
		}
		spin_lock_irqsave(&ss->lock, flags);

		mesg->status = 0;
		mesg->complete(mesg->context);
	}

	clear_fifo(ss);
	spi_sh_set_bit(ss, SPI_SH_SSD, SPI_SH_CR1);
	udelay(100);

	spi_sh_clear_bit(ss, SPI_SH_SSA | SPI_SH_SSDB | SPI_SH_SSD,
			 SPI_SH_CR1);

	clear_fifo(ss);

	spin_unlock_irqrestore(&ss->lock, flags);

	return;

 error:
	mesg->status = ret;
	mesg->complete(mesg->context);

	spi_sh_clear_bit(ss, SPI_SH_SSA | SPI_SH_SSDB | SPI_SH_SSD,
			 SPI_SH_CR1);
	clear_fifo(ss);

}

static int spi_sh_setup(struct spi_device *spi)
{
	struct spi_sh_data *ss = spi_master_get_devdata(spi->master);

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	pr_debug("%s: enter\n", __func__);

	spi_sh_write(ss, 0xfe, SPI_SH_CR1);	/* SPI sycle stop */
	spi_sh_write(ss, 0x00, SPI_SH_CR1);	/* CR1 init */
	spi_sh_write(ss, 0x00, SPI_SH_CR3);	/* CR3 init */

	clear_fifo(ss);

	/* 1/8 clock */
	spi_sh_write(ss, spi_sh_read(ss, SPI_SH_CR2) | 0x07, SPI_SH_CR2);
	udelay(10);

	return 0;
}

static int spi_sh_transfer(struct spi_device *spi, struct spi_message *mesg)
{
	struct spi_sh_data *ss = spi_master_get_devdata(spi->master);
	unsigned long flags;

	pr_debug("%s: enter\n", __func__);
	pr_debug("\tmode = %02x\n", spi->mode);

	spin_lock_irqsave(&ss->lock, flags);

	mesg->actual_length = 0;
	mesg->status = -EINPROGRESS;

	spi_sh_clear_bit(ss, SPI_SH_SSA, SPI_SH_CR1);

	list_add_tail(&mesg->queue, &ss->queue);
	queue_work(ss->workqueue, &ss->ws);

	spin_unlock_irqrestore(&ss->lock, flags);

	return 0;
}

static void spi_sh_cleanup(struct spi_device *spi)
{
	struct spi_sh_data *ss = spi_master_get_devdata(spi->master);

	pr_debug("%s: enter\n", __func__);

	spi_sh_clear_bit(ss, SPI_SH_SSA | SPI_SH_SSDB | SPI_SH_SSD,
			 SPI_SH_CR1);
}

static irqreturn_t spi_sh_irq(int irq, void *_ss)
{
	struct spi_sh_data *ss = (struct spi_sh_data *)_ss;
	unsigned long cr1;

	cr1 = spi_sh_read(ss, SPI_SH_CR1);
	if (cr1 & SPI_SH_TBE)
		ss->cr1 |= SPI_SH_TBE;
	if (cr1 & SPI_SH_TBF)
		ss->cr1 |= SPI_SH_TBF;
	if (cr1 & SPI_SH_RBE)
		ss->cr1 |= SPI_SH_RBE;
	if (cr1 & SPI_SH_RBF)
		ss->cr1 |= SPI_SH_RBF;

	if (ss->cr1) {
		spi_sh_clear_bit(ss, ss->cr1, SPI_SH_CR4);
		wake_up(&ss->wait);
	}

	return IRQ_HANDLED;
}

static int __devexit spi_sh_remove(struct platform_device *pdev)
{
	struct spi_sh_data *ss = dev_get_drvdata(&pdev->dev);

	destroy_workqueue(ss->workqueue);
	free_irq(ss->irq, ss);
	iounmap(ss->addr);
	spi_master_put(ss->master);

	return 0;
}

static int __devinit spi_sh_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct spi_master *master;
	struct spi_sh_data *ss;
	int ret, irq;

	/* get base addr */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(res == NULL)) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq error\n");
		return -ENODEV;
	}

	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_sh_data));
	if (master == NULL) {
		dev_err(&pdev->dev, "spi_alloc_master error.\n");
		return -ENOMEM;
	}

	ss = spi_master_get_devdata(master);
	dev_set_drvdata(&pdev->dev, ss);

	ss->irq = irq;
	ss->master = master;
	ss->addr = ioremap(res->start, resource_size(res));
	if (ss->addr == NULL) {
		dev_err(&pdev->dev, "ioremap error.\n");
		ret = -ENOMEM;
		goto error1;
	}
	INIT_LIST_HEAD(&ss->queue);
	spin_lock_init(&ss->lock);
	INIT_WORK(&ss->ws, spi_sh_work);
	init_waitqueue_head(&ss->wait);
	ss->workqueue = create_singlethread_workqueue(
					dev_name(master->dev.parent));
	if (ss->workqueue == NULL) {
		dev_err(&pdev->dev, "create workqueue error\n");
		ret = -EBUSY;
		goto error2;
	}

	ret = request_irq(irq, spi_sh_irq, IRQF_DISABLED, "spi_sh", ss);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq error\n");
		goto error3;
	}

	master->num_chipselect = 2;
	master->bus_num = pdev->id;
	master->setup = spi_sh_setup;
	master->transfer = spi_sh_transfer;
	master->cleanup = spi_sh_cleanup;

	ret = spi_register_master(master);
	if (ret < 0) {
		printk(KERN_ERR "spi_register_master error.\n");
		goto error4;
	}

	return 0;

 error4:
	free_irq(irq, ss);
 error3:
	destroy_workqueue(ss->workqueue);
 error2:
	iounmap(ss->addr);
 error1:
	spi_master_put(master);

	return ret;
}

static struct platform_driver spi_sh_driver = {
	.probe = spi_sh_probe,
	.remove = __devexit_p(spi_sh_remove),
	.driver = {
		.name = "sh_spi",
		.owner = THIS_MODULE,
	},
};

static int __init spi_sh_init(void)
{
	return platform_driver_register(&spi_sh_driver);
}
module_init(spi_sh_init);

static void __exit spi_sh_exit(void)
{
	platform_driver_unregister(&spi_sh_driver);
}
module_exit(spi_sh_exit);

MODULE_DESCRIPTION("SH SPI bus driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yoshihiro Shimoda");
MODULE_ALIAS("platform:sh_spi");
