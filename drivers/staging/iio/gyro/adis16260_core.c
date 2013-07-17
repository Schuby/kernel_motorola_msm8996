/*
 * ADIS16260/ADIS16265 Programmable Digital Gyroscope Sensor Driver
 *
 * Copyright 2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/list.h>
#include <linux/module.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/imu/adis.h>

#define ADIS16260_STARTUP_DELAY	220 /* ms */

#define ADIS16260_FLASH_CNT  0x00 /* Flash memory write count */
#define ADIS16260_SUPPLY_OUT 0x02 /* Power supply measurement */
#define ADIS16260_GYRO_OUT   0x04 /* X-axis gyroscope output */
#define ADIS16260_AUX_ADC    0x0A /* analog input channel measurement */
#define ADIS16260_TEMP_OUT   0x0C /* internal temperature measurement */
#define ADIS16260_ANGL_OUT   0x0E /* angle displacement */
#define ADIS16260_GYRO_OFF   0x14 /* Calibration, offset/bias adjustment */
#define ADIS16260_GYRO_SCALE 0x16 /* Calibration, scale adjustment */
#define ADIS16260_ALM_MAG1   0x20 /* Alarm 1 magnitude/polarity setting */
#define ADIS16260_ALM_MAG2   0x22 /* Alarm 2 magnitude/polarity setting */
#define ADIS16260_ALM_SMPL1  0x24 /* Alarm 1 dynamic rate of change setting */
#define ADIS16260_ALM_SMPL2  0x26 /* Alarm 2 dynamic rate of change setting */
#define ADIS16260_ALM_CTRL   0x28 /* Alarm control */
#define ADIS16260_AUX_DAC    0x30 /* Auxiliary DAC data */
#define ADIS16260_GPIO_CTRL  0x32 /* Control, digital I/O line */
#define ADIS16260_MSC_CTRL   0x34 /* Control, data ready, self-test settings */
#define ADIS16260_SMPL_PRD   0x36 /* Control, internal sample rate */
#define ADIS16260_SENS_AVG   0x38 /* Control, dynamic range, filtering */
#define ADIS16260_SLP_CNT    0x3A /* Control, sleep mode initiation */
#define ADIS16260_DIAG_STAT  0x3C /* Diagnostic, error flags */
#define ADIS16260_GLOB_CMD   0x3E /* Control, global commands */
#define ADIS16260_LOT_ID1    0x52 /* Lot Identification Code 1 */
#define ADIS16260_LOT_ID2    0x54 /* Lot Identification Code 2 */
#define ADIS16260_PROD_ID    0x56 /* Product identifier;
				   * convert to decimal = 16,265/16,260 */
#define ADIS16260_SERIAL_NUM 0x58 /* Serial number */

#define ADIS16260_ERROR_ACTIVE			(1<<14)
#define ADIS16260_NEW_DATA			(1<<15)

/* MSC_CTRL */
#define ADIS16260_MSC_CTRL_MEM_TEST		(1<<11)
/* Internal self-test enable */
#define ADIS16260_MSC_CTRL_INT_SELF_TEST	(1<<10)
#define ADIS16260_MSC_CTRL_NEG_SELF_TEST	(1<<9)
#define ADIS16260_MSC_CTRL_POS_SELF_TEST	(1<<8)
#define ADIS16260_MSC_CTRL_DATA_RDY_EN		(1<<2)
#define ADIS16260_MSC_CTRL_DATA_RDY_POL_HIGH	(1<<1)
#define ADIS16260_MSC_CTRL_DATA_RDY_DIO2	(1<<0)

/* SMPL_PRD */
/* Time base (tB): 0 = 1.953 ms, 1 = 60.54 ms */
#define ADIS16260_SMPL_PRD_TIME_BASE	(1<<7)
#define ADIS16260_SMPL_PRD_DIV_MASK	0x7F

/* SLP_CNT */
#define ADIS16260_SLP_CNT_POWER_OFF     0x80

/* DIAG_STAT */
#define ADIS16260_DIAG_STAT_ALARM2	(1<<9)
#define ADIS16260_DIAG_STAT_ALARM1	(1<<8)
#define ADIS16260_DIAG_STAT_FLASH_CHK_BIT	6
#define ADIS16260_DIAG_STAT_SELF_TEST_BIT	5
#define ADIS16260_DIAG_STAT_OVERFLOW_BIT	4
#define ADIS16260_DIAG_STAT_SPI_FAIL_BIT	3
#define ADIS16260_DIAG_STAT_FLASH_UPT_BIT	2
#define ADIS16260_DIAG_STAT_POWER_HIGH_BIT	1
#define ADIS16260_DIAG_STAT_POWER_LOW_BIT	0

/* GLOB_CMD */
#define ADIS16260_GLOB_CMD_SW_RESET	(1<<7)
#define ADIS16260_GLOB_CMD_FLASH_UPD	(1<<3)
#define ADIS16260_GLOB_CMD_DAC_LATCH	(1<<2)
#define ADIS16260_GLOB_CMD_FAC_CALIB	(1<<1)
#define ADIS16260_GLOB_CMD_AUTO_NULL	(1<<0)

#define ADIS16260_SPI_SLOW	(u32)(300 * 1000)
#define ADIS16260_SPI_BURST	(u32)(1000 * 1000)
#define ADIS16260_SPI_FAST	(u32)(2000 * 1000)

/* At the moment triggers are only used for ring buffer
 * filling. This may change!
 */

#define ADIS16260_SCAN_GYRO	0
#define ADIS16260_SCAN_SUPPLY	1
#define ADIS16260_SCAN_AUX_ADC	2
#define ADIS16260_SCAN_TEMP	3
#define ADIS16260_SCAN_ANGL	4

static ssize_t adis16260_read_frequency_available(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct adis *adis = iio_priv(indio_dev);
	if (spi_get_device_id(adis->spi)->driver_data)
		return sprintf(buf, "%s\n", "0.129 ~ 256");
	else
		return sprintf(buf, "%s\n", "256 2048");
}

static ssize_t adis16260_read_frequency(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct adis *adis = iio_priv(indio_dev);
	int ret, len = 0;
	u16 t;
	int sps;
	ret = adis_read_reg_16(adis, ADIS16260_SMPL_PRD, &t);
	if (ret)
		return ret;

	if (spi_get_device_id(adis->spi)->driver_data) /* If an adis16251 */
		sps = (t & ADIS16260_SMPL_PRD_TIME_BASE) ? 8 : 256;
	else
		sps = (t & ADIS16260_SMPL_PRD_TIME_BASE) ? 66 : 2048;
	sps /= (t & ADIS16260_SMPL_PRD_DIV_MASK) + 1;
	len = sprintf(buf, "%d\n", sps);
	return len;
}

static ssize_t adis16260_write_frequency(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct adis *adis = iio_priv(indio_dev);
	long val;
	int ret;
	u8 t;

	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	if (val == 0)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	if (spi_get_device_id(adis->spi)->driver_data) {
		t = (256 / val);
		if (t > 0)
			t--;
		t &= ADIS16260_SMPL_PRD_DIV_MASK;
	} else {
		t = (2048 / val);
		if (t > 0)
			t--;
		t &= ADIS16260_SMPL_PRD_DIV_MASK;
	}
	if ((t & ADIS16260_SMPL_PRD_DIV_MASK) >= 0x0A)
		adis->spi->max_speed_hz = ADIS16260_SPI_SLOW;
	else
		adis->spi->max_speed_hz = ADIS16260_SPI_FAST;
	ret = adis_write_reg_8(adis, ADIS16260_SMPL_PRD, t);

	mutex_unlock(&indio_dev->mlock);

	return ret ? ret : len;
}

/* Power down the device */
static int adis16260_stop_device(struct iio_dev *indio_dev)
{
	struct adis *adis = iio_priv(indio_dev);
	int ret;
	u16 val = ADIS16260_SLP_CNT_POWER_OFF;

	ret = adis_write_reg_16(adis, ADIS16260_SLP_CNT, val);
	if (ret)
		dev_err(&indio_dev->dev, "problem with turning device off: SLP_CNT");

	return ret;
}

static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
		adis16260_read_frequency,
		adis16260_write_frequency);

static IIO_DEVICE_ATTR(sampling_frequency_available,
		       S_IRUGO, adis16260_read_frequency_available, NULL, 0);

static const struct iio_chan_spec adis16260_channels[] = {
	ADIS_GYRO_CHAN(X, ADIS16260_GYRO_OUT, ADIS16260_SCAN_GYRO,
		BIT(IIO_CHAN_INFO_CALIBBIAS) |
		BIT(IIO_CHAN_INFO_CALIBSCALE), 14),
	ADIS_INCLI_CHAN(X, ADIS16260_ANGL_OUT, ADIS16260_SCAN_ANGL, 0, 14),
	ADIS_TEMP_CHAN(ADIS16260_TEMP_OUT, ADIS16260_SCAN_TEMP, 12),
	ADIS_SUPPLY_CHAN(ADIS16260_SUPPLY_OUT, ADIS16260_SCAN_SUPPLY, 12),
	ADIS_AUX_ADC_CHAN(ADIS16260_AUX_ADC, ADIS16260_SCAN_AUX_ADC, 12),
	IIO_CHAN_SOFT_TIMESTAMP(5),
};

static const u8 adis16260_addresses[][2] = {
	[ADIS16260_SCAN_GYRO] = { ADIS16260_GYRO_OFF, ADIS16260_GYRO_SCALE },
};

static int adis16260_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val, int *val2,
			      long mask)
{
	struct adis *adis = iio_priv(indio_dev);
	int ret;
	u8 addr;
	s16 val16;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return adis_single_conversion(indio_dev, chan,
				ADIS16260_ERROR_ACTIVE, val);
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:
			*val = 0;
			if (spi_get_device_id(adis->spi)->driver_data) {
				/* 0.01832 degree / sec */
				*val2 = IIO_DEGREE_TO_RAD(18320);
			} else {
				/* 0.07326 degree / sec */
				*val2 = IIO_DEGREE_TO_RAD(73260);
			}
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_VOLTAGE:
			if (chan->channel == 0) {
				*val = 1;
				*val2 = 831500; /* 1.8315 mV */
			} else {
				*val = 0;
				*val2 = 610500; /* 610.5 uV */
			}
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_TEMP:
			*val = 145;
			*val2 = 300000; /* 0.1453 C */
			return IIO_VAL_INT_PLUS_MICRO;
		default:
			return -EINVAL;
		}
		break;
	case IIO_CHAN_INFO_OFFSET:
		*val = 250000 / 1453; /* 25 C = 0x00 */
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBBIAS:
		addr = adis16260_addresses[chan->scan_index][0];
		ret = adis_read_reg_16(adis, addr, &val16);
		if (ret)
			return ret;

		*val = sign_extend32(val16, 11);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBSCALE:
		addr = adis16260_addresses[chan->scan_index][1];
		ret = adis_read_reg_16(adis, addr, &val16);
		if (ret)
			return ret;

		*val = val16;
		return IIO_VAL_INT;
	}
	return -EINVAL;
}

static int adis16260_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	struct adis *adis = iio_priv(indio_dev);
	u8 addr;

	switch (mask) {
	case IIO_CHAN_INFO_CALIBBIAS:
		if (val < -2048 || val >= 2048)
			return -EINVAL;

		addr = adis16260_addresses[chan->scan_index][0];
		return adis_write_reg_16(adis, addr, val);
	case IIO_CHAN_INFO_CALIBSCALE:
		if (val < 0 || val >= 4096)
			return -EINVAL;

		addr = adis16260_addresses[chan->scan_index][1];
		return adis_write_reg_16(adis, addr, val);
	}
	return -EINVAL;
}

static struct attribute *adis16260_attributes[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	NULL
};

static const struct attribute_group adis16260_attribute_group = {
	.attrs = adis16260_attributes,
};

static const struct iio_info adis16260_info = {
	.attrs = &adis16260_attribute_group,
	.read_raw = &adis16260_read_raw,
	.write_raw = &adis16260_write_raw,
	.update_scan_mode = adis_update_scan_mode,
	.driver_module = THIS_MODULE,
};

static const char * const adis1620_status_error_msgs[] = {
	[ADIS16260_DIAG_STAT_FLASH_CHK_BIT] = "Flash checksum error",
	[ADIS16260_DIAG_STAT_SELF_TEST_BIT] = "Self test error",
	[ADIS16260_DIAG_STAT_OVERFLOW_BIT] = "Sensor overrange",
	[ADIS16260_DIAG_STAT_SPI_FAIL_BIT] = "SPI failure",
	[ADIS16260_DIAG_STAT_FLASH_UPT_BIT] = "Flash update failed",
	[ADIS16260_DIAG_STAT_POWER_HIGH_BIT] = "Power supply above 5.25",
	[ADIS16260_DIAG_STAT_POWER_LOW_BIT] = "Power supply below 4.75",
};

static const struct adis_data adis16260_data = {
	.write_delay = 30,
	.read_delay = 30,
	.msc_ctrl_reg = ADIS16260_MSC_CTRL,
	.glob_cmd_reg = ADIS16260_GLOB_CMD,
	.diag_stat_reg = ADIS16260_DIAG_STAT,

	.self_test_mask = ADIS16260_MSC_CTRL_MEM_TEST,
	.startup_delay = ADIS16260_STARTUP_DELAY,

	.status_error_msgs = adis1620_status_error_msgs,
	.status_error_mask = BIT(ADIS16260_DIAG_STAT_FLASH_CHK_BIT) |
		BIT(ADIS16260_DIAG_STAT_SELF_TEST_BIT) |
		BIT(ADIS16260_DIAG_STAT_OVERFLOW_BIT) |
		BIT(ADIS16260_DIAG_STAT_SPI_FAIL_BIT) |
		BIT(ADIS16260_DIAG_STAT_FLASH_UPT_BIT) |
		BIT(ADIS16260_DIAG_STAT_POWER_HIGH_BIT) |
		BIT(ADIS16260_DIAG_STAT_POWER_LOW_BIT),
};

static int adis16260_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct adis *adis;
	int ret;

	/* setup the industrialio driver allocated elements */
	indio_dev = iio_device_alloc(sizeof(*adis));
	if (indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	adis = iio_priv(indio_dev);
	/* this is only used for removal purposes */
	spi_set_drvdata(spi, indio_dev);

	indio_dev->name = spi_get_device_id(spi)->name;
	indio_dev->dev.parent = &spi->dev;
	indio_dev->info = &adis16260_info;
	indio_dev->channels = adis16260_channels;
	indio_dev->num_channels = ARRAY_SIZE(adis16260_channels);
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = adis_init(adis, indio_dev, spi, &adis16260_data);
	if (ret)
		goto error_free_dev;

	ret = adis_setup_buffer_and_trigger(adis, indio_dev, NULL);
	if (ret)
		goto error_free_dev;

	/* Get the device into a sane initial state */
	ret = adis_initial_startup(adis);
	if (ret)
		goto error_cleanup_buffer_trigger;
	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_cleanup_buffer_trigger;

	return 0;

error_cleanup_buffer_trigger:
	adis_cleanup_buffer_and_trigger(adis, indio_dev);
error_free_dev:
	iio_device_free(indio_dev);
error_ret:
	return ret;
}

static int adis16260_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct adis *adis = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	adis16260_stop_device(indio_dev);
	adis_cleanup_buffer_and_trigger(adis, indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

/*
 * These parts do not need to be differentiated until someone adds
 * support for the on chip filtering.
 */
static const struct spi_device_id adis16260_id[] = {
	{"adis16260", 0},
	{"adis16265", 0},
	{"adis16250", 0},
	{"adis16255", 0},
	{"adis16251", 1},
	{}
};
MODULE_DEVICE_TABLE(spi, adis16260_id);

static struct spi_driver adis16260_driver = {
	.driver = {
		.name = "adis16260",
		.owner = THIS_MODULE,
	},
	.probe = adis16260_probe,
	.remove = adis16260_remove,
	.id_table = adis16260_id,
};
module_spi_driver(adis16260_driver);

MODULE_AUTHOR("Barry Song <21cnbao@gmail.com>");
MODULE_DESCRIPTION("Analog Devices ADIS16260/5 Digital Gyroscope Sensor");
MODULE_LICENSE("GPL v2");
