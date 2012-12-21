/*
 * Samsung S5P/EXYNOS4 SoC series MIPI-CSI receiver driver
 *
 * Copyright (C) 2011 - 2012 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <linux/platform_data/mipi-csis.h>
#include "mipi-csis.h"

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

/* Register map definition */

/* CSIS global control */
#define S5PCSIS_CTRL			0x00
#define S5PCSIS_CTRL_DPDN_DEFAULT	(0 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP		(1 << 31)
#define S5PCSIS_CTRL_ALIGN_32BIT	(1 << 20)
#define S5PCSIS_CTRL_UPDATE_SHADOW	(1 << 16)
#define S5PCSIS_CTRL_WCLK_EXTCLK	(1 << 8)
#define S5PCSIS_CTRL_RESET		(1 << 4)
#define S5PCSIS_CTRL_ENABLE		(1 << 0)

/* D-PHY control */
#define S5PCSIS_DPHYCTRL		0x04
#define S5PCSIS_DPHYCTRL_HSS_MASK	(0x1f << 27)
#define S5PCSIS_DPHYCTRL_ENABLE		(0x1f << 0)

#define S5PCSIS_CONFIG			0x08
#define S5PCSIS_CFG_FMT_YCBCR422_8BIT	(0x1e << 2)
#define S5PCSIS_CFG_FMT_RAW8		(0x2a << 2)
#define S5PCSIS_CFG_FMT_RAW10		(0x2b << 2)
#define S5PCSIS_CFG_FMT_RAW12		(0x2c << 2)
/* User defined formats, x = 1...4 */
#define S5PCSIS_CFG_FMT_USER(x)		((0x30 + x - 1) << 2)
#define S5PCSIS_CFG_FMT_MASK		(0x3f << 2)
#define S5PCSIS_CFG_NR_LANE_MASK	3

/* Interrupt mask */
#define S5PCSIS_INTMSK			0x10
#define S5PCSIS_INTMSK_EN_ALL		0xf000103f
#define S5PCSIS_INTMSK_EVEN_BEFORE	(1 << 31)
#define S5PCSIS_INTMSK_EVEN_AFTER	(1 << 30)
#define S5PCSIS_INTMSK_ODD_BEFORE	(1 << 29)
#define S5PCSIS_INTMSK_ODD_AFTER	(1 << 28)
#define S5PCSIS_INTMSK_ERR_SOT_HS	(1 << 12)
#define S5PCSIS_INTMSK_ERR_LOST_FS	(1 << 5)
#define S5PCSIS_INTMSK_ERR_LOST_FE	(1 << 4)
#define S5PCSIS_INTMSK_ERR_OVER		(1 << 3)
#define S5PCSIS_INTMSK_ERR_ECC		(1 << 2)
#define S5PCSIS_INTMSK_ERR_CRC		(1 << 1)
#define S5PCSIS_INTMSK_ERR_UNKNOWN	(1 << 0)

/* Interrupt source */
#define S5PCSIS_INTSRC			0x14
#define S5PCSIS_INTSRC_EVEN_BEFORE	(1 << 31)
#define S5PCSIS_INTSRC_EVEN_AFTER	(1 << 30)
#define S5PCSIS_INTSRC_EVEN		(0x3 << 30)
#define S5PCSIS_INTSRC_ODD_BEFORE	(1 << 29)
#define S5PCSIS_INTSRC_ODD_AFTER	(1 << 28)
#define S5PCSIS_INTSRC_ODD		(0x3 << 28)
#define S5PCSIS_INTSRC_NON_IMAGE_DATA	(0xff << 28)
#define S5PCSIS_INTSRC_ERR_SOT_HS	(0xf << 12)
#define S5PCSIS_INTSRC_ERR_LOST_FS	(1 << 5)
#define S5PCSIS_INTSRC_ERR_LOST_FE	(1 << 4)
#define S5PCSIS_INTSRC_ERR_OVER		(1 << 3)
#define S5PCSIS_INTSRC_ERR_ECC		(1 << 2)
#define S5PCSIS_INTSRC_ERR_CRC		(1 << 1)
#define S5PCSIS_INTSRC_ERR_UNKNOWN	(1 << 0)
#define S5PCSIS_INTSRC_ERRORS		0xf03f

/* Pixel resolution */
#define S5PCSIS_RESOL			0x2c
#define CSIS_MAX_PIX_WIDTH		0xffff
#define CSIS_MAX_PIX_HEIGHT		0xffff

/* Non-image packet data buffers */
#define S5PCSIS_PKTDATA_ODD		0x2000
#define S5PCSIS_PKTDATA_EVEN		0x3000
#define S5PCSIS_PKTDATA_SIZE		SZ_4K

enum {
	CSIS_CLK_MUX,
	CSIS_CLK_GATE,
};

static char *csi_clock_name[] = {
	[CSIS_CLK_MUX]  = "sclk_csis",
	[CSIS_CLK_GATE] = "csis",
};
#define NUM_CSIS_CLOCKS	ARRAY_SIZE(csi_clock_name)

static const char * const csis_supply_name[] = {
	"vddcore",  /* CSIS Core (1.0V, 1.1V or 1.2V) suppply */
	"vddio",    /* CSIS I/O and PLL (1.8V) supply */
};
#define CSIS_NUM_SUPPLIES ARRAY_SIZE(csis_supply_name)

enum {
	ST_POWERED	= 1,
	ST_STREAMING	= 2,
	ST_SUSPENDED	= 4,
};

struct s5pcsis_event {
	u32 mask;
	const char * const name;
	unsigned int counter;
};

static const struct s5pcsis_event s5pcsis_events[] = {
	/* Errors */
	{ S5PCSIS_INTSRC_ERR_SOT_HS,	"SOT Error" },
	{ S5PCSIS_INTSRC_ERR_LOST_FS,	"Lost Frame Start Error" },
	{ S5PCSIS_INTSRC_ERR_LOST_FE,	"Lost Frame End Error" },
	{ S5PCSIS_INTSRC_ERR_OVER,	"FIFO Overflow Error" },
	{ S5PCSIS_INTSRC_ERR_ECC,	"ECC Error" },
	{ S5PCSIS_INTSRC_ERR_CRC,	"CRC Error" },
	{ S5PCSIS_INTSRC_ERR_UNKNOWN,	"Unknown Error" },
	/* Non-image data receive events */
	{ S5PCSIS_INTSRC_EVEN_BEFORE,	"Non-image data before even frame" },
	{ S5PCSIS_INTSRC_EVEN_AFTER,	"Non-image data after even frame" },
	{ S5PCSIS_INTSRC_ODD_BEFORE,	"Non-image data before odd frame" },
	{ S5PCSIS_INTSRC_ODD_AFTER,	"Non-image data after odd frame" },
};
#define S5PCSIS_NUM_EVENTS ARRAY_SIZE(s5pcsis_events)

struct csis_pktbuf {
	u32 *data;
	unsigned int len;
};

/**
 * struct csis_state - the driver's internal state data structure
 * @lock: mutex serializing the subdev and power management operations,
 *        protecting @format and @flags members
 * @pads: CSIS pads array
 * @sd: v4l2_subdev associated with CSIS device instance
 * @index: the hardware instance index
 * @pdev: CSIS platform device
 * @regs: mmaped I/O registers memory
 * @supplies: CSIS regulator supplies
 * @clock: CSIS clocks
 * @irq: requested s5p-mipi-csis irq number
 * @flags: the state variable for power and streaming control
 * @csis_fmt: current CSIS pixel format
 * @format: common media bus format for the source and sink pad
 * @slock: spinlock protecting structure members below
 * @pkt_buf: the frame embedded (non-image) data buffer
 * @events: MIPI-CSIS event (error) counters
 */
struct csis_state {
	struct mutex lock;
	struct media_pad pads[CSIS_PADS_NUM];
	struct v4l2_subdev sd;
	u8 index;
	struct platform_device *pdev;
	void __iomem *regs;
	struct regulator_bulk_data supplies[CSIS_NUM_SUPPLIES];
	struct clk *clock[NUM_CSIS_CLOCKS];
	int irq;
	u32 flags;
	const struct csis_pix_format *csis_fmt;
	struct v4l2_mbus_framefmt format;

	struct spinlock slock;
	struct csis_pktbuf pkt_buf;
	struct s5pcsis_event events[S5PCSIS_NUM_EVENTS];
};

/**
 * struct csis_pix_format - CSIS pixel format description
 * @pix_width_alignment: horizontal pixel alignment, width will be
 *                       multiple of 2^pix_width_alignment
 * @code: corresponding media bus code
 * @fmt_reg: S5PCSIS_CONFIG register value
 * @data_alignment: MIPI-CSI data alignment in bits
 */
struct csis_pix_format {
	unsigned int pix_width_alignment;
	enum v4l2_mbus_pixelcode code;
	u32 fmt_reg;
	u8 data_alignment;
};

static const struct csis_pix_format s5pcsis_formats[] = {
	{
		.code = V4L2_MBUS_FMT_VYUY8_2X8,
		.fmt_reg = S5PCSIS_CFG_FMT_YCBCR422_8BIT,
		.data_alignment = 32,
	}, {
		.code = V4L2_MBUS_FMT_JPEG_1X8,
		.fmt_reg = S5PCSIS_CFG_FMT_USER(1),
		.data_alignment = 32,
	}, {
		.code = V4L2_MBUS_FMT_S5C_UYVY_JPEG_1X8,
		.fmt_reg = S5PCSIS_CFG_FMT_USER(1),
		.data_alignment = 32,
	}
};

#define s5pcsis_write(__csis, __r, __v) writel(__v, __csis->regs + __r)
#define s5pcsis_read(__csis, __r) readl(__csis->regs + __r)

static struct csis_state *sd_to_csis_state(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct csis_state, sd);
}

static const struct csis_pix_format *find_csis_format(
	struct v4l2_mbus_framefmt *mf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(s5pcsis_formats); i++)
		if (mf->code == s5pcsis_formats[i].code)
			return &s5pcsis_formats[i];
	return NULL;
}

static void s5pcsis_enable_interrupts(struct csis_state *state, bool on)
{
	u32 val = s5pcsis_read(state, S5PCSIS_INTMSK);

	val = on ? val | S5PCSIS_INTMSK_EN_ALL :
		   val & ~S5PCSIS_INTMSK_EN_ALL;
	s5pcsis_write(state, S5PCSIS_INTMSK, val);
}

static void s5pcsis_reset(struct csis_state *state)
{
	u32 val = s5pcsis_read(state, S5PCSIS_CTRL);

	s5pcsis_write(state, S5PCSIS_CTRL, val | S5PCSIS_CTRL_RESET);
	udelay(10);
}

static void s5pcsis_system_enable(struct csis_state *state, int on)
{
	u32 val;

	val = s5pcsis_read(state, S5PCSIS_CTRL);
	if (on)
		val |= S5PCSIS_CTRL_ENABLE;
	else
		val &= ~S5PCSIS_CTRL_ENABLE;
	s5pcsis_write(state, S5PCSIS_CTRL, val);

	val = s5pcsis_read(state, S5PCSIS_DPHYCTRL);
	if (on)
		val |= S5PCSIS_DPHYCTRL_ENABLE;
	else
		val &= ~S5PCSIS_DPHYCTRL_ENABLE;
	s5pcsis_write(state, S5PCSIS_DPHYCTRL, val);
}

/* Called with the state.lock mutex held */
static void __s5pcsis_set_format(struct csis_state *state)
{
	struct v4l2_mbus_framefmt *mf = &state->format;
	u32 val;

	v4l2_dbg(1, debug, &state->sd, "fmt: %#x, %d x %d\n",
		 mf->code, mf->width, mf->height);

	/* Color format */
	val = s5pcsis_read(state, S5PCSIS_CONFIG);
	val = (val & ~S5PCSIS_CFG_FMT_MASK) | state->csis_fmt->fmt_reg;
	s5pcsis_write(state, S5PCSIS_CONFIG, val);

	/* Pixel resolution */
	val = (mf->width << 16) | mf->height;
	s5pcsis_write(state, S5PCSIS_RESOL, val);
}

static void s5pcsis_set_hsync_settle(struct csis_state *state, int settle)
{
	u32 val = s5pcsis_read(state, S5PCSIS_DPHYCTRL);

	val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (settle << 27);
	s5pcsis_write(state, S5PCSIS_DPHYCTRL, val);
}

static void s5pcsis_set_params(struct csis_state *state)
{
	struct s5p_platform_mipi_csis *pdata = state->pdev->dev.platform_data;
	u32 val;

	val = s5pcsis_read(state, S5PCSIS_CONFIG);
	val = (val & ~S5PCSIS_CFG_NR_LANE_MASK) | (pdata->lanes - 1);
	s5pcsis_write(state, S5PCSIS_CONFIG, val);

	__s5pcsis_set_format(state);
	s5pcsis_set_hsync_settle(state, pdata->hs_settle);

	val = s5pcsis_read(state, S5PCSIS_CTRL);
	if (state->csis_fmt->data_alignment == 32)
		val |= S5PCSIS_CTRL_ALIGN_32BIT;
	else /* 24-bits */
		val &= ~S5PCSIS_CTRL_ALIGN_32BIT;

	val &= ~S5PCSIS_CTRL_WCLK_EXTCLK;
	if (pdata->wclk_source)
		val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	s5pcsis_write(state, S5PCSIS_CTRL, val);

	/* Update the shadow register. */
	val = s5pcsis_read(state, S5PCSIS_CTRL);
	s5pcsis_write(state, S5PCSIS_CTRL, val | S5PCSIS_CTRL_UPDATE_SHADOW);
}

static void s5pcsis_clk_put(struct csis_state *state)
{
	int i;

	for (i = 0; i < NUM_CSIS_CLOCKS; i++) {
		if (IS_ERR_OR_NULL(state->clock[i]))
			continue;
		clk_unprepare(state->clock[i]);
		clk_put(state->clock[i]);
		state->clock[i] = NULL;
	}
}

static int s5pcsis_clk_get(struct csis_state *state)
{
	struct device *dev = &state->pdev->dev;
	int i, ret;

	for (i = 0; i < NUM_CSIS_CLOCKS; i++) {
		state->clock[i] = clk_get(dev, csi_clock_name[i]);
		if (IS_ERR(state->clock[i]))
			goto err;
		ret = clk_prepare(state->clock[i]);
		if (ret < 0) {
			clk_put(state->clock[i]);
			state->clock[i] = NULL;
			goto err;
		}
	}
	return 0;
err:
	s5pcsis_clk_put(state);
	dev_err(dev, "failed to get clock: %s\n", csi_clock_name[i]);
	return -ENXIO;
}

static void s5pcsis_start_stream(struct csis_state *state)
{
	s5pcsis_reset(state);
	s5pcsis_set_params(state);
	s5pcsis_system_enable(state, true);
	s5pcsis_enable_interrupts(state, true);
}

static void s5pcsis_stop_stream(struct csis_state *state)
{
	s5pcsis_enable_interrupts(state, false);
	s5pcsis_system_enable(state, false);
}

static void s5pcsis_clear_counters(struct csis_state *state)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&state->slock, flags);
	for (i = 0; i < S5PCSIS_NUM_EVENTS; i++)
		state->events[i].counter = 0;
	spin_unlock_irqrestore(&state->slock, flags);
}

static void s5pcsis_log_counters(struct csis_state *state, bool non_errors)
{
	int i = non_errors ? S5PCSIS_NUM_EVENTS : S5PCSIS_NUM_EVENTS - 4;
	unsigned long flags;

	spin_lock_irqsave(&state->slock, flags);

	for (i--; i >= 0; i--)
		if (state->events[i].counter >= 0)
			v4l2_info(&state->sd, "%s events: %d\n",
				  state->events[i].name,
				  state->events[i].counter);

	spin_unlock_irqrestore(&state->slock, flags);
}

/*
 * V4L2 subdev operations
 */
static int s5pcsis_s_power(struct v4l2_subdev *sd, int on)
{
	struct csis_state *state = sd_to_csis_state(sd);
	struct device *dev = &state->pdev->dev;

	if (on)
		return pm_runtime_get_sync(dev);

	return pm_runtime_put_sync(dev);
}

static int s5pcsis_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct csis_state *state = sd_to_csis_state(sd);
	int ret = 0;

	v4l2_dbg(1, debug, sd, "%s: %d, state: 0x%x\n",
		 __func__, enable, state->flags);

	if (enable) {
		s5pcsis_clear_counters(state);
		ret = pm_runtime_get_sync(&state->pdev->dev);
		if (ret && ret != 1)
			return ret;
	}

	mutex_lock(&state->lock);
	if (enable) {
		if (state->flags & ST_SUSPENDED) {
			ret = -EBUSY;
			goto unlock;
		}
		s5pcsis_start_stream(state);
		state->flags |= ST_STREAMING;
	} else {
		s5pcsis_stop_stream(state);
		state->flags &= ~ST_STREAMING;
		if (debug > 0)
			s5pcsis_log_counters(state, true);
	}
unlock:
	mutex_unlock(&state->lock);
	if (!enable)
		pm_runtime_put(&state->pdev->dev);

	return ret == 1 ? 0 : ret;
}

static int s5pcsis_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(s5pcsis_formats))
		return -EINVAL;

	code->code = s5pcsis_formats[code->index].code;
	return 0;
}

static struct csis_pix_format const *s5pcsis_try_format(
	struct v4l2_mbus_framefmt *mf)
{
	struct csis_pix_format const *csis_fmt;

	csis_fmt = find_csis_format(mf);
	if (csis_fmt == NULL)
		csis_fmt = &s5pcsis_formats[0];

	mf->code = csis_fmt->code;
	v4l_bound_align_image(&mf->width, 1, CSIS_MAX_PIX_WIDTH,
			      csis_fmt->pix_width_alignment,
			      &mf->height, 1, CSIS_MAX_PIX_HEIGHT, 1,
			      0);
	return csis_fmt;
}

static struct v4l2_mbus_framefmt *__s5pcsis_get_format(
		struct csis_state *state, struct v4l2_subdev_fh *fh,
		u32 pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, pad) : NULL;

	return &state->format;
}

static int s5pcsis_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			   struct v4l2_subdev_format *fmt)
{
	struct csis_state *state = sd_to_csis_state(sd);
	struct csis_pix_format const *csis_fmt;
	struct v4l2_mbus_framefmt *mf;

	if (fmt->pad != CSIS_PAD_SOURCE && fmt->pad != CSIS_PAD_SINK)
		return -EINVAL;

	mf = __s5pcsis_get_format(state, fh, fmt->pad, fmt->which);

	if (fmt->pad == CSIS_PAD_SOURCE) {
		if (mf) {
			mutex_lock(&state->lock);
			fmt->format = *mf;
			mutex_unlock(&state->lock);
		}
		return 0;
	}
	csis_fmt = s5pcsis_try_format(&fmt->format);
	if (mf) {
		mutex_lock(&state->lock);
		*mf = fmt->format;
		if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
			state->csis_fmt = csis_fmt;
		mutex_unlock(&state->lock);
	}
	return 0;
}

static int s5pcsis_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			   struct v4l2_subdev_format *fmt)
{
	struct csis_state *state = sd_to_csis_state(sd);
	struct v4l2_mbus_framefmt *mf;

	if (fmt->pad != CSIS_PAD_SOURCE && fmt->pad != CSIS_PAD_SINK)
		return -EINVAL;

	mf = __s5pcsis_get_format(state, fh, fmt->pad, fmt->which);
	if (!mf)
		return -EINVAL;

	mutex_lock(&state->lock);
	fmt->format = *mf;
	mutex_unlock(&state->lock);
	return 0;
}

static int s5pcsis_s_rx_buffer(struct v4l2_subdev *sd, void *buf,
			       unsigned int *size)
{
	struct csis_state *state = sd_to_csis_state(sd);
	unsigned long flags;

	*size = min_t(unsigned int, *size, S5PCSIS_PKTDATA_SIZE);

	spin_lock_irqsave(&state->slock, flags);
	state->pkt_buf.data = buf;
	state->pkt_buf.len = *size;
	spin_unlock_irqrestore(&state->slock, flags);

	return 0;
}

static int s5pcsis_log_status(struct v4l2_subdev *sd)
{
	struct csis_state *state = sd_to_csis_state(sd);

	s5pcsis_log_counters(state, true);
	return 0;
}

static int s5pcsis_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *format = v4l2_subdev_get_try_format(fh, 0);

	format->colorspace = V4L2_COLORSPACE_JPEG;
	format->code = s5pcsis_formats[0].code;
	format->width = S5PCSIS_DEF_PIX_WIDTH;
	format->height = S5PCSIS_DEF_PIX_HEIGHT;
	format->field = V4L2_FIELD_NONE;

	return 0;
}

static const struct v4l2_subdev_internal_ops s5pcsis_sd_internal_ops = {
	.open = s5pcsis_open,
};

static struct v4l2_subdev_core_ops s5pcsis_core_ops = {
	.s_power = s5pcsis_s_power,
	.log_status = s5pcsis_log_status,
};

static struct v4l2_subdev_pad_ops s5pcsis_pad_ops = {
	.enum_mbus_code = s5pcsis_enum_mbus_code,
	.get_fmt = s5pcsis_get_fmt,
	.set_fmt = s5pcsis_set_fmt,
};

static struct v4l2_subdev_video_ops s5pcsis_video_ops = {
	.s_rx_buffer = s5pcsis_s_rx_buffer,
	.s_stream = s5pcsis_s_stream,
};

static struct v4l2_subdev_ops s5pcsis_subdev_ops = {
	.core = &s5pcsis_core_ops,
	.pad = &s5pcsis_pad_ops,
	.video = &s5pcsis_video_ops,
};

static irqreturn_t s5pcsis_irq_handler(int irq, void *dev_id)
{
	struct csis_state *state = dev_id;
	struct csis_pktbuf *pktbuf = &state->pkt_buf;
	unsigned long flags;
	u32 status;

	status = s5pcsis_read(state, S5PCSIS_INTSRC);
	spin_lock_irqsave(&state->slock, flags);

	if ((status & S5PCSIS_INTSRC_NON_IMAGE_DATA) && pktbuf->data) {
		u32 offset;

		if (status & S5PCSIS_INTSRC_EVEN)
			offset = S5PCSIS_PKTDATA_EVEN;
		else
			offset = S5PCSIS_PKTDATA_ODD;

		memcpy(pktbuf->data, state->regs + offset, pktbuf->len);
		pktbuf->data = NULL;
		rmb();
	}

	/* Update the event/error counters */
	if ((status & S5PCSIS_INTSRC_ERRORS) || debug) {
		int i;
		for (i = 0; i < S5PCSIS_NUM_EVENTS; i++) {
			if (!(status & state->events[i].mask))
				continue;
			state->events[i].counter++;
			v4l2_dbg(2, debug, &state->sd, "%s: %d\n",
				 state->events[i].name,
				 state->events[i].counter);
		}
		v4l2_dbg(2, debug, &state->sd, "status: %08x\n", status);
	}
	spin_unlock_irqrestore(&state->slock, flags);

	s5pcsis_write(state, S5PCSIS_INTSRC, status);
	return IRQ_HANDLED;
}

static int s5pcsis_probe(struct platform_device *pdev)
{
	struct s5p_platform_mipi_csis *pdata;
	struct resource *mem_res;
	struct csis_state *state;
	int ret = -ENOMEM;
	int i;

	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	mutex_init(&state->lock);
	spin_lock_init(&state->slock);

	state->pdev = pdev;
	state->index = max(0, pdev->id);

	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&pdev->dev, "Platform data not fully specified\n");
		return -EINVAL;
	}

	if ((state->index == 1 && pdata->lanes > CSIS1_MAX_LANES) ||
	    pdata->lanes > CSIS0_MAX_LANES) {
		dev_err(&pdev->dev, "Unsupported number of data lanes: %d\n",
			pdata->lanes);
		return -EINVAL;
	}

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	state->regs = devm_request_and_ioremap(&pdev->dev, mem_res);
	if (state->regs == NULL) {
		dev_err(&pdev->dev, "Failed to request and remap io memory\n");
		return -ENXIO;
	}

	state->irq = platform_get_irq(pdev, 0);
	if (state->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		return state->irq;
	}

	for (i = 0; i < CSIS_NUM_SUPPLIES; i++)
		state->supplies[i].supply = csis_supply_name[i];

	ret = regulator_bulk_get(&pdev->dev, CSIS_NUM_SUPPLIES,
				 state->supplies);
	if (ret)
		return ret;

	ret = s5pcsis_clk_get(state);
	if (ret)
		goto e_clkput;

	clk_enable(state->clock[CSIS_CLK_MUX]);
	if (pdata->clk_rate)
		clk_set_rate(state->clock[CSIS_CLK_MUX], pdata->clk_rate);
	else
		dev_WARN(&pdev->dev, "No clock frequency specified!\n");

	ret = devm_request_irq(&pdev->dev, state->irq, s5pcsis_irq_handler,
			       0, dev_name(&pdev->dev), state);
	if (ret) {
		dev_err(&pdev->dev, "Interrupt request failed\n");
		goto e_regput;
	}

	v4l2_subdev_init(&state->sd, &s5pcsis_subdev_ops);
	state->sd.owner = THIS_MODULE;
	strlcpy(state->sd.name, dev_name(&pdev->dev), sizeof(state->sd.name));
	state->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->csis_fmt = &s5pcsis_formats[0];

	state->format.code = s5pcsis_formats[0].code;
	state->format.width = S5PCSIS_DEF_PIX_WIDTH;
	state->format.height = S5PCSIS_DEF_PIX_HEIGHT;

	state->pads[CSIS_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	state->pads[CSIS_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&state->sd.entity,
				CSIS_PADS_NUM, state->pads, 0);
	if (ret < 0)
		goto e_clkput;

	/* This allows to retrieve the platform device id by the host driver */
	v4l2_set_subdevdata(&state->sd, pdev);

	/* .. and a pointer to the subdev. */
	platform_set_drvdata(pdev, &state->sd);

	memcpy(state->events, s5pcsis_events, sizeof(state->events));

	pm_runtime_enable(&pdev->dev);
	return 0;

e_regput:
	regulator_bulk_free(CSIS_NUM_SUPPLIES, state->supplies);
e_clkput:
	clk_disable(state->clock[CSIS_CLK_MUX]);
	s5pcsis_clk_put(state);
	return ret;
}

static int s5pcsis_pm_suspend(struct device *dev, bool runtime)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct csis_state *state = sd_to_csis_state(sd);
	int ret = 0;

	v4l2_dbg(1, debug, sd, "%s: flags: 0x%x\n",
		 __func__, state->flags);

	mutex_lock(&state->lock);
	if (state->flags & ST_POWERED) {
		s5pcsis_stop_stream(state);
		ret = s5p_csis_phy_enable(state->index, false);
		if (ret)
			goto unlock;
		ret = regulator_bulk_disable(CSIS_NUM_SUPPLIES,
					     state->supplies);
		if (ret)
			goto unlock;
		clk_disable(state->clock[CSIS_CLK_GATE]);
		state->flags &= ~ST_POWERED;
		if (!runtime)
			state->flags |= ST_SUSPENDED;
	}
 unlock:
	mutex_unlock(&state->lock);
	return ret ? -EAGAIN : 0;
}

static int s5pcsis_pm_resume(struct device *dev, bool runtime)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct csis_state *state = sd_to_csis_state(sd);
	int ret = 0;

	v4l2_dbg(1, debug, sd, "%s: flags: 0x%x\n",
		 __func__, state->flags);

	mutex_lock(&state->lock);
	if (!runtime && !(state->flags & ST_SUSPENDED))
		goto unlock;

	if (!(state->flags & ST_POWERED)) {
		ret = regulator_bulk_enable(CSIS_NUM_SUPPLIES,
					    state->supplies);
		if (ret)
			goto unlock;
		ret = s5p_csis_phy_enable(state->index, true);
		if (!ret) {
			state->flags |= ST_POWERED;
		} else {
			regulator_bulk_disable(CSIS_NUM_SUPPLIES,
					       state->supplies);
			goto unlock;
		}
		clk_enable(state->clock[CSIS_CLK_GATE]);
	}
	if (state->flags & ST_STREAMING)
		s5pcsis_start_stream(state);

	state->flags &= ~ST_SUSPENDED;
 unlock:
	mutex_unlock(&state->lock);
	return ret ? -EAGAIN : 0;
}

#ifdef CONFIG_PM_SLEEP
static int s5pcsis_suspend(struct device *dev)
{
	return s5pcsis_pm_suspend(dev, false);
}

static int s5pcsis_resume(struct device *dev)
{
	return s5pcsis_pm_resume(dev, false);
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int s5pcsis_runtime_suspend(struct device *dev)
{
	return s5pcsis_pm_suspend(dev, true);
}

static int s5pcsis_runtime_resume(struct device *dev)
{
	return s5pcsis_pm_resume(dev, true);
}
#endif

static int s5pcsis_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct csis_state *state = sd_to_csis_state(sd);

	pm_runtime_disable(&pdev->dev);
	s5pcsis_pm_suspend(&pdev->dev, false);
	clk_disable(state->clock[CSIS_CLK_MUX]);
	pm_runtime_set_suspended(&pdev->dev);
	s5pcsis_clk_put(state);
	regulator_bulk_free(CSIS_NUM_SUPPLIES, state->supplies);

	media_entity_cleanup(&state->sd.entity);

	return 0;
}

static const struct dev_pm_ops s5pcsis_pm_ops = {
	SET_RUNTIME_PM_OPS(s5pcsis_runtime_suspend, s5pcsis_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(s5pcsis_suspend, s5pcsis_resume)
};

static struct platform_driver s5pcsis_driver = {
	.probe		= s5pcsis_probe,
	.remove		= s5pcsis_remove,
	.driver		= {
		.name	= CSIS_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &s5pcsis_pm_ops,
	},
};

module_platform_driver(s5pcsis_driver);

MODULE_AUTHOR("Sylwester Nawrocki <s.nawrocki@samsung.com>");
MODULE_DESCRIPTION("Samsung S5P/EXYNOS SoC MIPI-CSI2 receiver driver");
MODULE_LICENSE("GPL");
