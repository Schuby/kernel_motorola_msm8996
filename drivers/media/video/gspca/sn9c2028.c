/*
 * SN9C2028 library
 *
 * Copyright (C) 2009 Theodore Kilgore <kilgota@auburn.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define MODULE_NAME "sn9c2028"

#include "gspca.h"

MODULE_AUTHOR("Theodore Kilgore");
MODULE_DESCRIPTION("Sonix SN9C2028 USB Camera Driver");
MODULE_LICENSE("GPL");

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;  /* !! must be the first item */
	u8 sof_read;
	u16 model;
};

struct init_command {
	unsigned char instruction[6];
	unsigned char to_read; /* length to read. 0 means no reply requested */
};

/* V4L2 controls supported by the driver */
static struct ctrl sd_ctrls[] = {
};

/* How to change the resolution of any of the VGA cams is unknown */
static const struct v4l2_pix_format vga_mode[] = {
	{640, 480, V4L2_PIX_FMT_SN9C2028, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

/* No way to change the resolution of the CIF cams is known */
static const struct v4l2_pix_format cif_mode[] = {
	{352, 288, V4L2_PIX_FMT_SN9C2028, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3 / 4,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

/* the bytes to write are in gspca_dev->usb_buf */
static int sn9c2028_command(struct gspca_dev *gspca_dev, u8 *command)
{
	int rc;

	PDEBUG(D_USBO, "sending command %02x%02x%02x%02x%02x%02x", command[0],
	       command[1], command[2], command[3], command[4], command[5]);

	memcpy(gspca_dev->usb_buf, command, 6);
	rc = usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			USB_REQ_GET_CONFIGURATION,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			2, 0, gspca_dev->usb_buf, 6, 500);
	if (rc < 0) {
		PDEBUG(D_ERR, "command write [%02x] error %d",
				gspca_dev->usb_buf[0], rc);
		return rc;
	}

	return 0;
}

static int sn9c2028_read1(struct gspca_dev *gspca_dev)
{
	int rc;

	rc = usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			USB_REQ_GET_STATUS,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			1, 0, gspca_dev->usb_buf, 1, 500);
	if (rc != 1) {
		PDEBUG(D_ERR, "read1 error %d", rc);
		return (rc < 0) ? rc : -EIO;
	}
	PDEBUG(D_USBI, "read1 response %02x", gspca_dev->usb_buf[0]);
	return gspca_dev->usb_buf[0];
}

static int sn9c2028_read4(struct gspca_dev *gspca_dev, u8 *reading)
{
	int rc;
	rc = usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			USB_REQ_GET_STATUS,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			4, 0, gspca_dev->usb_buf, 4, 500);
	if (rc != 4) {
		PDEBUG(D_ERR, "read4 error %d", rc);
		return (rc < 0) ? rc : -EIO;
	}
	memcpy(reading, gspca_dev->usb_buf, 4);
	PDEBUG(D_USBI, "read4 response %02x%02x%02x%02x", reading[0],
	       reading[1], reading[2], reading[3]);
	return rc;
}

static int sn9c2028_long_command(struct gspca_dev *gspca_dev, u8 *command)
{
	int i, status;
	__u8 reading[4];

	status = sn9c2028_command(gspca_dev, command);
	if (status < 0)
		return status;

	status = -1;
	for (i = 0; i < 256 && status < 2; i++)
		status = sn9c2028_read1(gspca_dev);
	if (status != 2) {
		PDEBUG(D_ERR, "long command status read error %d", status);
		return (status < 0) ? status : -EIO;
	}

	memset(reading, 0, 4);
	status = sn9c2028_read4(gspca_dev, reading);
	if (status < 0)
		return status;

	/* in general, the first byte of the response is the first byte of
	 * the command, or'ed with 8 */
	status = sn9c2028_read1(gspca_dev);
	if (status < 0)
		return status;

	return 0;
}

static int sn9c2028_short_command(struct gspca_dev *gspca_dev, u8 *command)
{
	int err_code;

	err_code = sn9c2028_command(gspca_dev, command);
	if (err_code < 0)
		return err_code;

	err_code = sn9c2028_read1(gspca_dev);
	if (err_code < 0)
		return err_code;

	return 0;
}

/* this function is called at probe time */
static int sd_config(struct gspca_dev *gspca_dev,
		     const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam = &gspca_dev->cam;

	PDEBUG(D_PROBE, "SN9C2028 camera detected (vid/pid 0x%04X:0x%04X)",
	       id->idVendor, id->idProduct);

	sd->model = id->idProduct;

	switch (sd->model) {
	case 0x7005:
		PDEBUG(D_PROBE, "Genius Smart 300 camera");
		break;
	case 0x8000:
		PDEBUG(D_PROBE, "DC31VC");
		break;
	case 0x8001:
		PDEBUG(D_PROBE, "Spy camera");
		break;
	case 0x8003:
		PDEBUG(D_PROBE, "CIF camera");
		break;
	case 0x8008:
		PDEBUG(D_PROBE, "Mini-Shotz ms-350 camera");
		break;
	case 0x800a:
		PDEBUG(D_PROBE, "Vivitar 3350b type camera");
		cam->input_flags = V4L2_IN_ST_VFLIP | V4L2_IN_ST_HFLIP;
		break;
	}

	switch (sd->model) {
	case 0x8000:
	case 0x8001:
	case 0x8003:
		cam->cam_mode = cif_mode;
		cam->nmodes = ARRAY_SIZE(cif_mode);
		break;
	default:
		cam->cam_mode = vga_mode;
		cam->nmodes = ARRAY_SIZE(vga_mode);
	}
	return 0;
}

/* this function is called at probe and resume time */
static int sd_init(struct gspca_dev *gspca_dev)
{
	int status = -1;

	sn9c2028_read1(gspca_dev);
	sn9c2028_read1(gspca_dev);
	status = sn9c2028_read1(gspca_dev);

	return (status < 0) ? status : 0;
}

static int run_start_commands(struct gspca_dev *gspca_dev,
			      struct init_command *cam_commands, int n)
{
	int i, err_code = -1;

	for (i = 0; i < n; i++) {
		switch (cam_commands[i].to_read) {
		case 4:
			err_code = sn9c2028_long_command(gspca_dev,
					cam_commands[i].instruction);
			break;
		case 1:
			err_code = sn9c2028_short_command(gspca_dev,
					cam_commands[i].instruction);
			break;
		case 0:
			err_code = sn9c2028_command(gspca_dev,
					cam_commands[i].instruction);
			break;
		}
		if (err_code < 0)
			return err_code;
	}
	return 0;
}

static int start_spy_cam(struct gspca_dev *gspca_dev)
{
	struct init_command spy_start_commands[] = {
		{{0x0c, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x20, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x21, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x22, 0x01, 0x04, 0x00, 0x00}, 4},
		{{0x13, 0x23, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x24, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x16, 0x00, 0x00}, 4}, /* width  352 */
		{{0x13, 0x26, 0x01, 0x12, 0x00, 0x00}, 4}, /* height 288 */
		/* {{0x13, 0x27, 0x01, 0x28, 0x00, 0x00}, 4}, */
		{{0x13, 0x27, 0x01, 0x68, 0x00, 0x00}, 4},
		{{0x13, 0x28, 0x01, 0x09, 0x00, 0x00}, 4}, /* red gain ?*/
		/* {{0x13, 0x28, 0x01, 0x00, 0x00, 0x00}, 4}, */
		{{0x13, 0x29, 0x01, 0x00, 0x00, 0x00}, 4},
		/* {{0x13, 0x29, 0x01, 0x0c, 0x00, 0x00}, 4}, */
		{{0x13, 0x2a, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2b, 0x01, 0x00, 0x00, 0x00}, 4},
		/* {{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4}, */
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x02, 0x00, 0x00}, 4},
		/* {{0x13, 0x2e, 0x01, 0x09, 0x00, 0x00}, 4}, */
		{{0x13, 0x2e, 0x01, 0x09, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x07, 0x00, 0x00}, 4},
		{{0x12, 0x34, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x34, 0x01, 0xa1, 0x00, 0x00}, 4},
		{{0x13, 0x35, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x02, 0x06, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x03, 0x13, 0x00, 0x00, 0x00}, 4}, /*don't mess with*/
		/*{{0x11, 0x04, 0x06, 0x00, 0x00, 0x00}, 4}, observed */
		{{0x11, 0x04, 0x00, 0x00, 0x00, 0x00}, 4}, /* brighter */
		/*{{0x11, 0x05, 0x65, 0x00, 0x00, 0x00}, 4}, observed */
		{{0x11, 0x05, 0x00, 0x00, 0x00, 0x00}, 4}, /* brighter */
		{{0x11, 0x06, 0xb1, 0x00, 0x00, 0x00}, 4}, /* observed */
		{{0x11, 0x07, 0x00, 0x00, 0x00, 0x00}, 4},
		/*{{0x11, 0x08, 0x06, 0x00, 0x00, 0x00}, 4}, observed */
		{{0x11, 0x08, 0x0b, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x09, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0a, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0b, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0c, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0d, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0e, 0x04, 0x00, 0x00, 0x00}, 4},
		/* {{0x11, 0x0f, 0x00, 0x00, 0x00, 0x00}, 4}, */
		/* brightness or gain. 0 is default. 4 is good
		 * indoors at night with incandescent lighting */
		{{0x11, 0x0f, 0x04, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x10, 0x06, 0x00, 0x00, 0x00}, 4}, /*hstart or hoffs*/
		{{0x11, 0x11, 0x06, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x12, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x14, 0x02, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x13, 0x01, 0x00, 0x00, 0x00}, 4},
		/* {{0x1b, 0x02, 0x06, 0x00, 0x00, 0x00}, 1}, observed */
		{{0x1b, 0x02, 0x11, 0x00, 0x00, 0x00}, 1}, /* brighter */
		/* {{0x1b, 0x13, 0x01, 0x00, 0x00, 0x00}, 1}, observed */
		{{0x1b, 0x13, 0x11, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x34, 0xa1, 0x00, 0x00, 0x00}, 1}, /* compresses */
		/* Camera should start to capture now. */
	};

	return run_start_commands(gspca_dev, spy_start_commands,
				  ARRAY_SIZE(spy_start_commands));
}

static int start_cif_cam(struct gspca_dev *gspca_dev)
{
	struct init_command cif_start_commands[] = {
		{{0x0c, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		/* The entire sequence below seems redundant */
		/* {{0x13, 0x20, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x21, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x22, 0x01, 0x06, 0x00, 0x00}, 4},
		{{0x13, 0x23, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x24, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x16, 0x00, 0x00}, 4}, width?
		{{0x13, 0x26, 0x01, 0x12, 0x00, 0x00}, 4}, height?
		{{0x13, 0x27, 0x01, 0x68, 0x00, 0x00}, 4}, subsample?
		{{0x13, 0x28, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x29, 0x01, 0x20, 0x00, 0x00}, 4},
		{{0x13, 0x2a, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2b, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x0f, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x0c, 0x00, 0x00}, 4},
		{{0x12, 0x34, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x34, 0x01, 0xa1, 0x00, 0x00}, 4},
		{{0x13, 0x35, 0x01, 0x00, 0x00, 0x00}, 4},*/
		{{0x1b, 0x21, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x17, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x19, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x02, 0x06, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x03, 0x5a, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x04, 0x27, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x05, 0x01, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x12, 0x14, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x13, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x14, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x15, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x16, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x77, 0xa2, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x06, 0x0f, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x07, 0x14, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x08, 0x0f, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x09, 0x10, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x0e, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x0f, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x12, 0x07, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x10, 0x1f, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x11, 0x01, 0x00, 0x00, 0x00}, 1},
		{{0x13, 0x25, 0x01, 0x16, 0x00, 0x00}, 1}, /* width/8 */
		{{0x13, 0x26, 0x01, 0x12, 0x00, 0x00}, 1}, /* height/8 */
		/* {{0x13, 0x27, 0x01, 0x68, 0x00, 0x00}, 4}, subsample?
		 * {{0x13, 0x28, 0x01, 0x1e, 0x00, 0x00}, 4}, does nothing
		 * {{0x13, 0x27, 0x01, 0x20, 0x00, 0x00}, 4}, */
		/* {{0x13, 0x29, 0x01, 0x22, 0x00, 0x00}, 4},
		 * causes subsampling
		 * but not a change in the resolution setting! */
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x01, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x08, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x06, 0x00, 0x00}, 4},
		{{0x13, 0x28, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x1b, 0x04, 0x6d, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x05, 0x03, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x36, 0x06, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x0e, 0x01, 0x00, 0x00, 0x00}, 1},
		{{0x12, 0x27, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x1b, 0x0f, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x36, 0x05, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x10, 0x0f, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x02, 0x06, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x11, 0x01, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x34, 0xa1, 0x00, 0x00, 0x00}, 1},/* use compression */
		/* Camera should start to capture now. */
	};

	return run_start_commands(gspca_dev, cif_start_commands,
				  ARRAY_SIZE(cif_start_commands));
}

static int start_ms350_cam(struct gspca_dev *gspca_dev)
{
	struct init_command ms350_start_commands[] = {
		{{0x0c, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x16, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x20, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x21, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x22, 0x01, 0x04, 0x00, 0x00}, 4},
		{{0x13, 0x23, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x24, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x16, 0x00, 0x00}, 4},
		{{0x13, 0x26, 0x01, 0x12, 0x00, 0x00}, 4},
		{{0x13, 0x27, 0x01, 0x28, 0x00, 0x00}, 4},
		{{0x13, 0x28, 0x01, 0x09, 0x00, 0x00}, 4},
		{{0x13, 0x29, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2a, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2b, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x0f, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x0c, 0x00, 0x00}, 4},
		{{0x12, 0x34, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x34, 0x01, 0xa1, 0x00, 0x00}, 4},
		{{0x13, 0x35, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x00, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x01, 0x70, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x02, 0x05, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x03, 0x5d, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x04, 0x07, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x05, 0x25, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x06, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x07, 0x09, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x08, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x09, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0a, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0b, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0c, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0d, 0x0c, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0e, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x0f, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x10, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x11, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x12, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x13, 0x63, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x15, 0x70, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x18, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x11, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x28, 0x00, 0x00}, 4}, /* width  */
		{{0x13, 0x26, 0x01, 0x1e, 0x00, 0x00}, 4}, /* height */
		{{0x13, 0x28, 0x01, 0x09, 0x00, 0x00}, 4}, /* vstart? */
		{{0x13, 0x27, 0x01, 0x28, 0x00, 0x00}, 4},
		{{0x13, 0x29, 0x01, 0x40, 0x00, 0x00}, 4}, /* hstart? */
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x0f, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x0c, 0x00, 0x00}, 4},
		{{0x1b, 0x02, 0x05, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x11, 0x01, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x18, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x02, 0x0a, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x11, 0x01, 0x00, 0x00, 0x00}, 0},
		/* Camera should start to capture now. */
	};

	return run_start_commands(gspca_dev, ms350_start_commands,
				  ARRAY_SIZE(ms350_start_commands));
}

static int start_genius_cam(struct gspca_dev *gspca_dev)
{
	struct init_command genius_start_commands[] = {
		{{0x0c, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x16, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x10, 0x00, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x16, 0x00, 0x00}, 4},
		{{0x13, 0x26, 0x01, 0x12, 0x00, 0x00}, 4},
		/* "preliminary" width and height settings */
		{{0x13, 0x28, 0x01, 0x0e, 0x00, 0x00}, 4},
		{{0x13, 0x27, 0x01, 0x20, 0x00, 0x00}, 4},
		{{0x13, 0x29, 0x01, 0x22, 0x00, 0x00}, 4},
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x09, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x07, 0x00, 0x00}, 4},
		{{0x11, 0x20, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x21, 0x2d, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x22, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x23, 0x03, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x10, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x11, 0x64, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x12, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x13, 0x91, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x14, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x15, 0x20, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x16, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x17, 0x60, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x20, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x21, 0x2d, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x22, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x23, 0x03, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x25, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x26, 0x02, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x27, 0x88, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x30, 0x38, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x31, 0x2a, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x32, 0x2a, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x33, 0x2a, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x34, 0x02, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x5b, 0x0a, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x28, 0x00, 0x00}, 4}, /* real width */
		{{0x13, 0x26, 0x01, 0x1e, 0x00, 0x00}, 4}, /* real height */
		{{0x13, 0x28, 0x01, 0x0e, 0x00, 0x00}, 4},
		{{0x13, 0x27, 0x01, 0x20, 0x00, 0x00}, 4},
		{{0x13, 0x29, 0x01, 0x62, 0x00, 0x00}, 4},
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x0f, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x0c, 0x00, 0x00}, 4},
		{{0x11, 0x20, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x21, 0x2a, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x22, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x23, 0x28, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x10, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x11, 0x04, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x12, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x13, 0x03, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x14, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x15, 0xe0, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x16, 0x02, 0x00, 0x00, 0x00}, 4},
		{{0x11, 0x17, 0x80, 0x00, 0x00, 0x00}, 4},
		{{0x1c, 0x20, 0x00, 0x2a, 0x00, 0x00}, 1},
		{{0x1c, 0x20, 0x00, 0x2a, 0x00, 0x00}, 1},
		{{0x20, 0x34, 0xa1, 0x00, 0x00, 0x00}, 0}
		/* Camera should start to capture now. */
	};

	return run_start_commands(gspca_dev, genius_start_commands,
				  ARRAY_SIZE(genius_start_commands));
}

static int start_vivitar_cam(struct gspca_dev *gspca_dev)
{
	struct init_command vivitar_start_commands[] = {
		{{0x0c, 0x01, 0x00, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x20, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x21, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x22, 0x01, 0x01, 0x00, 0x00}, 4},
		{{0x13, 0x23, 0x01, 0x01, 0x00, 0x00}, 4},
		{{0x13, 0x24, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x25, 0x01, 0x28, 0x00, 0x00}, 4},
		{{0x13, 0x26, 0x01, 0x1e, 0x00, 0x00}, 4},
		{{0x13, 0x27, 0x01, 0x20, 0x00, 0x00}, 4},
		{{0x13, 0x28, 0x01, 0x0a, 0x00, 0x00}, 4},
		/*
		 * Above is changed from OEM 0x0b. Fixes Bayer tiling.
		 * Presumably gives a vertical shift of one row.
		 */
		{{0x13, 0x29, 0x01, 0x20, 0x00, 0x00}, 4},
		/* Above seems to do horizontal shift. */
		{{0x13, 0x2a, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2b, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x2c, 0x01, 0x02, 0x00, 0x00}, 4},
		{{0x13, 0x2d, 0x01, 0x03, 0x00, 0x00}, 4},
		{{0x13, 0x2e, 0x01, 0x0f, 0x00, 0x00}, 4},
		{{0x13, 0x2f, 0x01, 0x0c, 0x00, 0x00}, 4},
		/* Above three commands seem to relate to brightness. */
		{{0x12, 0x34, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x13, 0x34, 0x01, 0xa1, 0x00, 0x00}, 4},
		{{0x13, 0x35, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x1b, 0x12, 0x80, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x01, 0x77, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x02, 0x3a, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x12, 0x78, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x13, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x14, 0x80, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x15, 0x34, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x1b, 0x04, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x20, 0x44, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x23, 0xee, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x26, 0xa0, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x27, 0x9a, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x28, 0xa0, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x29, 0x30, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x2a, 0x80, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x2b, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x2f, 0x3d, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x30, 0x24, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x32, 0x86, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x60, 0xa9, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x61, 0x42, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x65, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x69, 0x38, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x6f, 0x88, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x70, 0x0b, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x71, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x74, 0x21, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x75, 0x86, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x76, 0x00, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x7d, 0xf3, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x17, 0x1c, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x18, 0xc0, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x19, 0x05, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x1a, 0xf6, 0x00, 0x00, 0x00}, 1},
		/* {{0x13, 0x25, 0x01, 0x28, 0x00, 0x00}, 4},
		{{0x13, 0x26, 0x01, 0x1e, 0x00, 0x00}, 4},
		{{0x13, 0x28, 0x01, 0x0b, 0x00, 0x00}, 4}, */
		{{0x20, 0x36, 0x06, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x10, 0x26, 0x00, 0x00, 0x00}, 1},
		{{0x12, 0x27, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x1b, 0x76, 0x03, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x36, 0x05, 0x00, 0x00, 0x00}, 1},
		{{0x1b, 0x00, 0x3f, 0x00, 0x00, 0x00}, 1},
		/* Above is brightness; OEM driver setting is 0x10 */
		{{0x12, 0x27, 0x01, 0x00, 0x00, 0x00}, 4},
		{{0x20, 0x29, 0x30, 0x00, 0x00, 0x00}, 1},
		{{0x20, 0x34, 0xa1, 0x00, 0x00, 0x00}, 1}
	};

	return run_start_commands(gspca_dev, vivitar_start_commands,
				  ARRAY_SIZE(vivitar_start_commands));
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int err_code;

	sd->sof_read = 0;

	switch (sd->model) {
	case 0x7005:
		err_code = start_genius_cam(gspca_dev);
		break;
	case 0x8001:
		err_code = start_spy_cam(gspca_dev);
		break;
	case 0x8003:
		err_code = start_cif_cam(gspca_dev);
		break;
	case 0x8008:
		err_code = start_ms350_cam(gspca_dev);
		break;
	case 0x800a:
		err_code = start_vivitar_cam(gspca_dev);
		break;
	default:
		PDEBUG(D_ERR, "Starting unknown camera, please report this");
		return -ENXIO;
	}

	return err_code;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	int result;
	__u8 data[6];

	result = sn9c2028_read1(gspca_dev);
	if (result < 0)
		PDEBUG(D_ERR, "Camera Stop read failed");

	memset(data, 0, 6);
	data[0] = 0x14;
	result = sn9c2028_command(gspca_dev, data);
	if (result < 0)
		PDEBUG(D_ERR, "Camera Stop command failed");
}

/* Include sn9c2028 sof detection functions */
#include "sn9c2028.h"

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			__u8 *data,			/* isoc packet */
			int len)			/* iso packet length */
{
	unsigned char *sof;

	sof = sn9c2028_find_sof(gspca_dev, data, len);
	if (sof) {
		int n;

		/* finish decoding current frame */
		n = sof - data;
		if (n > sizeof sn9c2028_sof_marker)
			n -= sizeof sn9c2028_sof_marker;
		else
			n = 0;
		gspca_frame_add(gspca_dev, LAST_PACKET, data, n);
		/* Start next frame. */
		gspca_frame_add(gspca_dev, FIRST_PACKET,
			sn9c2028_sof_marker, sizeof sn9c2028_sof_marker);
		len -= sof - data;
		data = sof;
	}
	gspca_frame_add(gspca_dev, INTER_PACKET, data, len);
}

/* sub-driver description */
static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.init = sd_init,
	.start = sd_start,
	.stopN = sd_stopN,
	.pkt_scan = sd_pkt_scan,
};

/* -- module initialisation -- */
static const __devinitdata struct usb_device_id device_table[] = {
	{USB_DEVICE(0x0458, 0x7005)}, /* Genius Smart 300, version 2 */
	/* The Genius Smart is untested. I can't find an owner ! */
	/* {USB_DEVICE(0x0c45, 0x8000)}, DC31VC, Don't know this camera */
	{USB_DEVICE(0x0c45, 0x8001)}, /* Wild Planet digital spy cam */
	{USB_DEVICE(0x0c45, 0x8003)}, /* Several small CIF cameras */
	/* {USB_DEVICE(0x0c45, 0x8006)}, Unknown VGA camera */
	{USB_DEVICE(0x0c45, 0x8008)}, /* Mini-Shotz ms-350 */
	{USB_DEVICE(0x0c45, 0x800a)}, /* Vivicam 3350B */
	{}
};
MODULE_DEVICE_TABLE(usb, device_table);

/* -- device connect -- */
static int sd_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
			       THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend = gspca_suspend,
	.resume = gspca_resume,
#endif
};

/* -- module insert / remove -- */
static int __init sd_mod_init(void)
{
	int ret;

	ret = usb_register(&sd_driver);
	if (ret < 0)
		return ret;
	PDEBUG(D_PROBE, "registered");
	return 0;
}

static void __exit sd_mod_exit(void)
{
	usb_deregister(&sd_driver);
	PDEBUG(D_PROBE, "deregistered");
}

module_init(sd_mod_init);
module_exit(sd_mod_exit);
