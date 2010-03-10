/*
 * broadsheetfb.c -- FB driver for E-Ink Broadsheet controller
 *
 * Copyright (C) 2008, Jaya Kumar
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Layout is based on skeletonfb.c by James Simmons and Geert Uytterhoeven.
 *
 * This driver is written to be used with the Broadsheet display controller.
 *
 * It is intended to be architecture independent. A board specific driver
 * must be used to perform all the physical IO interactions.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/uaccess.h>

#include <video/broadsheetfb.h>

/* track panel specific parameters */
struct panel_info {
	int w;
	int h;
	u16 sdcfg;
	u16 gdcfg;
	u16 lutfmt;
	u16 fsynclen;
	u16 fendfbegin;
	u16 lsynclen;
	u16 lendlbegin;
	u16 pixclk;
};

/* table of panel specific parameters to be indexed into by the board drivers */
static struct panel_info panel_table[] = {
	{	/* standard 6" on TFT backplane */
		.w = 800,
		.h = 600,
		.sdcfg = (100 | (1 << 8) | (1 << 9)),
		.gdcfg = 2,
		.lutfmt = (4 | (1 << 7)),
		.fsynclen = 4,
		.fendfbegin = (10 << 8) | 4,
		.lsynclen = 10,
		.lendlbegin = (100 << 8) | 4,
		.pixclk = 6,
	},
	{	/* custom 3.7" flexible on PET or steel */
		.w = 320,
		.h = 240,
		.sdcfg = (67 | (0 << 8) | (0 << 9) | (0 << 10) | (0 << 12)),
		.gdcfg = 3,
		.lutfmt = (4 | (1 << 7)),
		.fsynclen = 0,
		.fendfbegin = (80 << 8) | 4,
		.lsynclen = 10,
		.lendlbegin = (80 << 8) | 20,
		.pixclk = 14,
	},
	{	/* standard 9.7" on TFT backplane */
		.w = 1200,
		.h = 825,
		.sdcfg = (100 | (1 << 8) | (1 << 9) | (0 << 10) | (0 << 12)),
		.gdcfg = 2,
		.lutfmt = (4 | (1 << 7)),
		.fsynclen = 0,
		.fendfbegin = (4 << 8) | 4,
		.lsynclen = 4,
		.lendlbegin = (60 << 8) | 10,
		.pixclk = 3,
	},
};

#define DPY_W 800
#define DPY_H 600

static struct fb_fix_screeninfo broadsheetfb_fix __devinitdata = {
	.id =		"broadsheetfb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_STATIC_PSEUDOCOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.line_length =	DPY_W,
	.accel =	FB_ACCEL_NONE,
};

static struct fb_var_screeninfo broadsheetfb_var __devinitdata = {
	.xres		= DPY_W,
	.yres		= DPY_H,
	.xres_virtual	= DPY_W,
	.yres_virtual	= DPY_H,
	.bits_per_pixel	= 8,
	.grayscale	= 1,
	.red =		{ 0, 4, 0 },
	.green =	{ 0, 4, 0 },
	.blue =		{ 0, 4, 0 },
	.transp =	{ 0, 0, 0 },
};

/* main broadsheetfb functions */
static void broadsheet_gpio_issue_data(struct broadsheetfb_par *par, u16 data)
{
	par->board->set_ctl(par, BS_WR, 0);
	par->board->set_hdb(par, data);
	par->board->set_ctl(par, BS_WR, 1);
}

static void broadsheet_gpio_issue_cmd(struct broadsheetfb_par *par, u16 data)
{
	par->board->set_ctl(par, BS_DC, 0);
	broadsheet_gpio_issue_data(par, data);
}

static void broadsheet_gpio_send_command(struct broadsheetfb_par *par, u16 data)
{
	par->board->wait_for_rdy(par);

	par->board->set_ctl(par, BS_CS, 0);
	broadsheet_gpio_issue_cmd(par, data);
	par->board->set_ctl(par, BS_DC, 1);
	par->board->set_ctl(par, BS_CS, 1);
}

static void broadsheet_gpio_send_cmdargs(struct broadsheetfb_par *par, u16 cmd,
					int argc, u16 *argv)
{
	int i;

	par->board->wait_for_rdy(par);

	par->board->set_ctl(par, BS_CS, 0);
	broadsheet_gpio_issue_cmd(par, cmd);
	par->board->set_ctl(par, BS_DC, 1);

	for (i = 0; i < argc; i++)
		broadsheet_gpio_issue_data(par, argv[i]);
	par->board->set_ctl(par, BS_CS, 1);
}

static void broadsheet_mmio_send_cmdargs(struct broadsheetfb_par *par, u16 cmd,
				    int argc, u16 *argv)
{
	int i;

	par->board->mmio_write(par, BS_MMIO_CMD, cmd);

	for (i = 0; i < argc; i++)
		par->board->mmio_write(par, BS_MMIO_DATA, argv[i]);
}

static void broadsheet_send_command(struct broadsheetfb_par *par, u16 data)
{
	if (par->board->mmio_write)
		par->board->mmio_write(par, BS_MMIO_CMD, data);
	else
		broadsheet_gpio_send_command(par, data);
}

static void broadsheet_send_cmdargs(struct broadsheetfb_par *par, u16 cmd,
				    int argc, u16 *argv)
{
	if (par->board->mmio_write)
		broadsheet_mmio_send_cmdargs(par, cmd, argc, argv);
	else
		broadsheet_gpio_send_cmdargs(par, cmd, argc, argv);
}

static void broadsheet_gpio_burst_write(struct broadsheetfb_par *par, int size,
					u16 *data)
{
	int i;
	u16 tmp;

	par->board->set_ctl(par, BS_CS, 0);
	par->board->set_ctl(par, BS_DC, 1);

	for (i = 0; i < size; i++) {
		par->board->set_ctl(par, BS_WR, 0);
		tmp = (data[i] & 0x0F) << 4;
		tmp |= (data[i] & 0x0F00) << 4;
		par->board->set_hdb(par, tmp);
		par->board->set_ctl(par, BS_WR, 1);
	}

	par->board->set_ctl(par, BS_CS, 1);
}

static void broadsheet_mmio_burst_write(struct broadsheetfb_par *par, int size,
				   u16 *data)
{
	int i;
	u16 tmp;

	for (i = 0; i < size; i++) {
		tmp = (data[i] & 0x0F) << 4;
		tmp |= (data[i] & 0x0F00) << 4;
		par->board->mmio_write(par, BS_MMIO_DATA, tmp);
	}

}

static void broadsheet_burst_write(struct broadsheetfb_par *par, int size,
				   u16 *data)
{
	if (par->board->mmio_write)
		broadsheet_mmio_burst_write(par, size, data);
	else
		broadsheet_gpio_burst_write(par, size, data);
}

static u16 broadsheet_gpio_get_data(struct broadsheetfb_par *par)
{
	u16 res;
	/* wait for ready to go hi. (lo is busy) */
	par->board->wait_for_rdy(par);

	/* cs lo, dc lo for cmd, we lo for each data, db as usual */
	par->board->set_ctl(par, BS_DC, 1);
	par->board->set_ctl(par, BS_CS, 0);
	par->board->set_ctl(par, BS_WR, 0);

	res = par->board->get_hdb(par);

	/* strobe wr */
	par->board->set_ctl(par, BS_WR, 1);
	par->board->set_ctl(par, BS_CS, 1);

	return res;
}


static u16 broadsheet_get_data(struct broadsheetfb_par *par)
{
	if (par->board->mmio_read)
		return par->board->mmio_read(par);
	else
		return broadsheet_gpio_get_data(par);
}

static void broadsheet_gpio_write_reg(struct broadsheetfb_par *par, u16 reg,
					u16 data)
{
	/* wait for ready to go hi. (lo is busy) */
	par->board->wait_for_rdy(par);

	/* cs lo, dc lo for cmd, we lo for each data, db as usual */
	par->board->set_ctl(par, BS_CS, 0);

	broadsheet_gpio_issue_cmd(par, BS_CMD_WR_REG);

	par->board->set_ctl(par, BS_DC, 1);

	broadsheet_gpio_issue_data(par, reg);
	broadsheet_gpio_issue_data(par, data);

	par->board->set_ctl(par, BS_CS, 1);
}

static void broadsheet_mmio_write_reg(struct broadsheetfb_par *par, u16 reg,
				 u16 data)
{
	par->board->mmio_write(par, BS_MMIO_CMD, BS_CMD_WR_REG);
	par->board->mmio_write(par, BS_MMIO_DATA, reg);
	par->board->mmio_write(par, BS_MMIO_DATA, data);

}

static void broadsheet_write_reg(struct broadsheetfb_par *par, u16 reg,
					u16 data)
{
	if (par->board->mmio_write)
		broadsheet_mmio_write_reg(par, reg, data);
	else
		broadsheet_gpio_write_reg(par, reg, data);
}

static void broadsheet_write_reg32(struct broadsheetfb_par *par, u16 reg,
					u32 data)
{
	broadsheet_write_reg(par, reg, cpu_to_le32(data) & 0xFFFF);
	broadsheet_write_reg(par, reg + 2, (cpu_to_le32(data) >> 16) & 0xFFFF);
}


static u16 broadsheet_read_reg(struct broadsheetfb_par *par, u16 reg)
{
	broadsheet_send_cmdargs(par, BS_CMD_RD_REG, 1, &reg);
	par->board->wait_for_rdy(par);
	return broadsheet_get_data(par);
}

static void __devinit broadsheet_init_display(struct broadsheetfb_par *par)
{
	u16 args[5];
	int xres = par->info->var.xres;
	int yres = par->info->var.yres;

	args[0] = panel_table[par->panel_index].w;
	args[1] = panel_table[par->panel_index].h;
	args[2] = panel_table[par->panel_index].sdcfg;
	args[3] = panel_table[par->panel_index].gdcfg;
	args[4] = panel_table[par->panel_index].lutfmt;
	broadsheet_send_cmdargs(par, BS_CMD_INIT_DSPE_CFG, 5, args);

	/* did the controller really set it? */
	broadsheet_send_cmdargs(par, BS_CMD_INIT_DSPE_CFG, 5, args);

	args[0] = panel_table[par->panel_index].fsynclen;
	args[1] = panel_table[par->panel_index].fendfbegin;
	args[2] = panel_table[par->panel_index].lsynclen;
	args[3] = panel_table[par->panel_index].lendlbegin;
	args[4] = panel_table[par->panel_index].pixclk;
	broadsheet_send_cmdargs(par, BS_CMD_INIT_DSPE_TMG, 5, args);

	broadsheet_write_reg32(par, 0x310, xres*yres*2);

	/* setup waveform */
	args[0] = 0x886;
	args[1] = 0;
	broadsheet_send_cmdargs(par, BS_CMD_RD_WFM_INFO, 2, args);

	broadsheet_send_command(par, BS_CMD_UPD_GDRV_CLR);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_TRG);

	broadsheet_write_reg(par, 0x330, 0x84);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_TRG);

	args[0] = (0x3 << 4);
	broadsheet_send_cmdargs(par, BS_CMD_LD_IMG, 1, args);

	args[0] = 0x154;
	broadsheet_send_cmdargs(par, BS_CMD_WR_REG, 1, args);

	broadsheet_burst_write(par, (panel_table[par->panel_index].w *
					panel_table[par->panel_index].h)/2,
					(u16 *) par->info->screen_base);

	broadsheet_send_command(par, BS_CMD_LD_IMG_END);

	args[0] = 0x4300;
	broadsheet_send_cmdargs(par, BS_CMD_UPD_FULL, 1, args);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_TRG);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_FREND);

	par->board->wait_for_rdy(par);
}

static void __devinit broadsheet_init(struct broadsheetfb_par *par)
{
	broadsheet_send_command(par, BS_CMD_INIT_SYS_RUN);
	/* the controller needs a second */
	msleep(1000);
	broadsheet_init_display(par);
}

static void broadsheetfb_dpy_update_pages(struct broadsheetfb_par *par,
						u16 y1, u16 y2)
{
	u16 args[5];
	unsigned char *buf = (unsigned char *)par->info->screen_base;

	/* y1 must be a multiple of 4 so drop the lower bits */
	y1 &= 0xFFFC;
	/* y2 must be a multiple of 4 , but - 1 so up the lower bits */
	y2 |= 0x0003;

	args[0] = 0x3 << 4;
	args[1] = 0;
	args[2] = y1;
	args[3] = cpu_to_le16(par->info->var.xres);
	args[4] = y2;
	broadsheet_send_cmdargs(par, BS_CMD_LD_IMG_AREA, 5, args);

	args[0] = 0x154;
	broadsheet_send_cmdargs(par, BS_CMD_WR_REG, 1, args);

	buf += y1 * par->info->var.xres;
	broadsheet_burst_write(par, ((1 + y2 - y1) * par->info->var.xres)/2,
				(u16 *) buf);

	broadsheet_send_command(par, BS_CMD_LD_IMG_END);

	args[0] = 0x4300;
	broadsheet_send_cmdargs(par, BS_CMD_UPD_FULL, 1, args);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_TRG);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_FREND);

	par->board->wait_for_rdy(par);

}

static void broadsheetfb_dpy_update(struct broadsheetfb_par *par)
{
	u16 args[5];

	args[0] = 0x3 << 4;
	broadsheet_send_cmdargs(par, BS_CMD_LD_IMG, 1, args);

	args[0] = 0x154;
	broadsheet_send_cmdargs(par, BS_CMD_WR_REG, 1, args);
	broadsheet_burst_write(par, (panel_table[par->panel_index].w *
					panel_table[par->panel_index].h)/2,
					(u16 *) par->info->screen_base);

	broadsheet_send_command(par, BS_CMD_LD_IMG_END);

	args[0] = 0x4300;
	broadsheet_send_cmdargs(par, BS_CMD_UPD_FULL, 1, args);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_TRG);

	broadsheet_send_command(par, BS_CMD_WAIT_DSPE_FREND);

	par->board->wait_for_rdy(par);

}

/* this is called back from the deferred io workqueue */
static void broadsheetfb_dpy_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	u16 y1 = 0, h = 0;
	int prev_index = -1;
	struct page *cur;
	struct fb_deferred_io *fbdefio = info->fbdefio;
	int h_inc;
	u16 yres = info->var.yres;
	u16 xres = info->var.xres;

	/* height increment is fixed per page */
	h_inc = DIV_ROUND_UP(PAGE_SIZE , xres);

	/* walk the written page list and swizzle the data */
	list_for_each_entry(cur, &fbdefio->pagelist, lru) {
		if (prev_index < 0) {
			/* just starting so assign first page */
			y1 = (cur->index << PAGE_SHIFT) / xres;
			h = h_inc;
		} else if ((prev_index + 1) == cur->index) {
			/* this page is consecutive so increase our height */
			h += h_inc;
		} else {
			/* page not consecutive, issue previous update first */
			broadsheetfb_dpy_update_pages(info->par, y1, y1 + h);
			/* start over with our non consecutive page */
			y1 = (cur->index << PAGE_SHIFT) / xres;
			h = h_inc;
		}
		prev_index = cur->index;
	}

	/* if we still have any pages to update we do so now */
	if (h >= yres) {
		/* its a full screen update, just do it */
		broadsheetfb_dpy_update(info->par);
	} else {
		broadsheetfb_dpy_update_pages(info->par, y1,
						min((u16) (y1 + h), yres));
	}
}

static void broadsheetfb_fillrect(struct fb_info *info,
				   const struct fb_fillrect *rect)
{
	struct broadsheetfb_par *par = info->par;

	sys_fillrect(info, rect);

	broadsheetfb_dpy_update(par);
}

static void broadsheetfb_copyarea(struct fb_info *info,
				   const struct fb_copyarea *area)
{
	struct broadsheetfb_par *par = info->par;

	sys_copyarea(info, area);

	broadsheetfb_dpy_update(par);
}

static void broadsheetfb_imageblit(struct fb_info *info,
				const struct fb_image *image)
{
	struct broadsheetfb_par *par = info->par;

	sys_imageblit(info, image);

	broadsheetfb_dpy_update(par);
}

/*
 * this is the slow path from userspace. they can seek and write to
 * the fb. it's inefficient to do anything less than a full screen draw
 */
static ssize_t broadsheetfb_write(struct fb_info *info, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct broadsheetfb_par *par = info->par;
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void *)(info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if  (!err)
		*ppos += count;

	broadsheetfb_dpy_update(par);

	return (err) ? err : count;
}

static struct fb_ops broadsheetfb_ops = {
	.owner		= THIS_MODULE,
	.fb_read        = fb_sys_read,
	.fb_write	= broadsheetfb_write,
	.fb_fillrect	= broadsheetfb_fillrect,
	.fb_copyarea	= broadsheetfb_copyarea,
	.fb_imageblit	= broadsheetfb_imageblit,
};

static struct fb_deferred_io broadsheetfb_defio = {
	.delay		= HZ/4,
	.deferred_io	= broadsheetfb_dpy_deferred_io,
};

static int __devinit broadsheetfb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	struct broadsheet_board *board;
	int retval = -ENOMEM;
	int videomemorysize;
	unsigned char *videomemory;
	struct broadsheetfb_par *par;
	int i;
	int dpyw, dpyh;
	int panel_index;

	/* pick up board specific routines */
	board = dev->dev.platform_data;
	if (!board)
		return -EINVAL;

	/* try to count device specific driver, if can't, platform recalls */
	if (!try_module_get(board->owner))
		return -ENODEV;

	info = framebuffer_alloc(sizeof(struct broadsheetfb_par), &dev->dev);
	if (!info)
		goto err;

	switch (board->get_panel_type()) {
	case 37:
		panel_index = 1;
		break;
	case 97:
		panel_index = 2;
		break;
	case 6:
	default:
		panel_index = 0;
		break;
	}

	dpyw = panel_table[panel_index].w;
	dpyh = panel_table[panel_index].h;

	videomemorysize = roundup((dpyw*dpyh), PAGE_SIZE);

	videomemory = vmalloc(videomemorysize);
	if (!videomemory)
		goto err_fb_rel;

	memset(videomemory, 0, videomemorysize);

	info->screen_base = (char *)videomemory;
	info->fbops = &broadsheetfb_ops;

	broadsheetfb_var.xres = dpyw;
	broadsheetfb_var.yres = dpyh;
	broadsheetfb_var.xres_virtual = dpyw;
	broadsheetfb_var.yres_virtual = dpyh;
	info->var = broadsheetfb_var;

	broadsheetfb_fix.line_length = dpyw;
	info->fix = broadsheetfb_fix;
	info->fix.smem_len = videomemorysize;
	par = info->par;
	par->panel_index = panel_index;
	par->info = info;
	par->board = board;
	par->write_reg = broadsheet_write_reg;
	par->read_reg = broadsheet_read_reg;
	init_waitqueue_head(&par->waitq);

	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

	info->fbdefio = &broadsheetfb_defio;
	fb_deferred_io_init(info);

	retval = fb_alloc_cmap(&info->cmap, 16, 0);
	if (retval < 0) {
		dev_err(&dev->dev, "Failed to allocate colormap\n");
		goto err_vfree;
	}

	/* set cmap */
	for (i = 0; i < 16; i++)
		info->cmap.red[i] = (((2*i)+1)*(0xFFFF))/32;
	memcpy(info->cmap.green, info->cmap.red, sizeof(u16)*16);
	memcpy(info->cmap.blue, info->cmap.red, sizeof(u16)*16);

	retval = par->board->setup_irq(info);
	if (retval < 0)
		goto err_cmap;

	/* this inits the dpy */
	retval = board->init(par);
	if (retval < 0)
		goto err_free_irq;

	broadsheet_init(par);

	retval = register_framebuffer(info);
	if (retval < 0)
		goto err_free_irq;
	platform_set_drvdata(dev, info);

	printk(KERN_INFO
	       "fb%d: Broadsheet frame buffer, using %dK of video memory\n",
	       info->node, videomemorysize >> 10);


	return 0;

err_free_irq:
	board->cleanup(par);
err_cmap:
	fb_dealloc_cmap(&info->cmap);
err_vfree:
	vfree(videomemory);
err_fb_rel:
	framebuffer_release(info);
err:
	module_put(board->owner);
	return retval;

}

static int __devexit broadsheetfb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		struct broadsheetfb_par *par = info->par;
		unregister_framebuffer(info);
		fb_deferred_io_cleanup(info);
		par->board->cleanup(par);
		fb_dealloc_cmap(&info->cmap);
		vfree((void *)info->screen_base);
		module_put(par->board->owner);
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver broadsheetfb_driver = {
	.probe	= broadsheetfb_probe,
	.remove = broadsheetfb_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "broadsheetfb",
	},
};

static int __init broadsheetfb_init(void)
{
	return platform_driver_register(&broadsheetfb_driver);
}

static void __exit broadsheetfb_exit(void)
{
	platform_driver_unregister(&broadsheetfb_driver);
}

module_init(broadsheetfb_init);
module_exit(broadsheetfb_exit);

MODULE_DESCRIPTION("fbdev driver for Broadsheet controller");
MODULE_AUTHOR("Jaya Kumar");
MODULE_LICENSE("GPL");
