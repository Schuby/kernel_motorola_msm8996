/*
 * Copyright (C) 2009 Renesas Solutions Corp.
 *
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/usb/r8a66597.h>
#include <asm/heartbeat.h>
#include <asm/sh_eth.h>
#include <cpu/sh7724.h>

/*
 *  Address      Interface        BusWidth
 *-----------------------------------------
 *  0x0000_0000  uboot            16bit
 *  0x0004_0000  Linux romImage   16bit
 *  0x0014_0000  MTD for Linux    16bit
 *  0x0400_0000  Internal I/O     16/32bit
 *  0x0800_0000  DRAM             32bit
 *  0x1800_0000  MFI              16bit
 */

/* Heartbeat */
static unsigned char led_pos[] = { 0, 1, 2, 3 };
static struct heartbeat_data heartbeat_data = {
	.regsize = 8,
	.nr_bits = 4,
	.bit_pos = led_pos,
};

static struct resource heartbeat_resources[] = {
	[0] = {
		.start  = 0xA405012C, /* PTG */
		.end    = 0xA405012E - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device heartbeat_device = {
	.name           = "heartbeat",
	.id             = -1,
	.dev = {
		.platform_data = &heartbeat_data,
	},
	.num_resources  = ARRAY_SIZE(heartbeat_resources),
	.resource       = heartbeat_resources,
};

/* MTD */
static struct mtd_partition nor_flash_partitions[] = {
	{
		.name = "boot loader",
		.offset = 0,
		.size = (5 * 1024 * 1024),
		.mask_flags = MTD_CAP_ROM,
	}, {
		.name = "free-area",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data nor_flash_data = {
	.width		= 2,
	.parts		= nor_flash_partitions,
	.nr_parts	= ARRAY_SIZE(nor_flash_partitions),
};

static struct resource nor_flash_resources[] = {
	[0] = {
		.name	= "NOR Flash",
		.start	= 0x00000000,
		.end	= 0x03ffffff,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device nor_flash_device = {
	.name		= "physmap-flash",
	.resource	= nor_flash_resources,
	.num_resources	= ARRAY_SIZE(nor_flash_resources),
	.dev		= {
		.platform_data = &nor_flash_data,
	},
};

/* SH Eth */
#define SH_ETH_ADDR	(0xA4600000)
#define SH_ETH_MAHR	(SH_ETH_ADDR + 0x1C0)
#define SH_ETH_MALR	(SH_ETH_ADDR + 0x1C8)
static struct resource sh_eth_resources[] = {
	[0] = {
		.start = SH_ETH_ADDR,
		.end   = SH_ETH_ADDR + 0x1FC,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 91,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

struct sh_eth_plat_data sh_eth_plat = {
	.phy = 0x1f, /* SMSC LAN8700 */
	.edmac_endian = EDMAC_LITTLE_ENDIAN,
};

static struct platform_device sh_eth_device = {
	.name = "sh-eth",
	.id	= 0,
	.dev = {
		.platform_data = &sh_eth_plat,
	},
	.num_resources = ARRAY_SIZE(sh_eth_resources),
	.resource = sh_eth_resources,
};

/* USB0 host */
void usb0_port_power(int port, int power)
{
	gpio_set_value(GPIO_PTB4, power);
}

static struct r8a66597_platdata usb0_host_data = {
	.on_chip = 1,
	.port_power = usb0_port_power,
};

static struct resource usb0_host_resources[] = {
	[0] = {
		.start	= 0xa4d80000,
		.end	= 0xa4d80124 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 65,
		.end	= 65,
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_LOW,
	},
};

static struct platform_device usb0_host_device = {
	.name		= "r8a66597_hcd",
	.id		= 0,
	.dev = {
		.dma_mask		= NULL,         /*  not use dma */
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &usb0_host_data,
	},
	.num_resources	= ARRAY_SIZE(usb0_host_resources),
	.resource	= usb0_host_resources,
};

/*
 * USB1
 *
 * CN5 can use both host/function,
 * and we can determine it by checking PTB[3]
 *
 * This time only USB1 host is supported.
 */
void usb1_port_power(int port, int power)
{
	if (!gpio_get_value(GPIO_PTB3)) {
		printk(KERN_ERR "USB1 function is not supported\n");
		return;
	}

	gpio_set_value(GPIO_PTB5, power);
}

static struct r8a66597_platdata usb1_host_data = {
	.on_chip = 1,
	.port_power = usb1_port_power,
};

static struct resource usb1_host_resources[] = {
	[0] = {
		.start	= 0xa4d90000,
		.end	= 0xa4d90124 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 66,
		.end	= 66,
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_LOW,
	},
};

static struct platform_device usb1_host_device = {
	.name		= "r8a66597_hcd",
	.id		= 1,
	.dev = {
		.dma_mask		= NULL,         /*  not use dma */
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &usb1_host_data,
	},
	.num_resources	= ARRAY_SIZE(usb1_host_resources),
	.resource	= usb1_host_resources,
};

static struct platform_device *ecovec_devices[] __initdata = {
	&heartbeat_device,
	&nor_flash_device,
	&sh_eth_device,
	&usb0_host_device,
	&usb1_host_device, /* USB1 host support */
};

static int __init devices_setup(void)
{
	/* enable SCIFA0 */
	gpio_request(GPIO_FN_SCIF0_TXD, NULL);
	gpio_request(GPIO_FN_SCIF0_RXD, NULL);

	/* enable debug LED */
	gpio_request(GPIO_PTG0, NULL);
	gpio_request(GPIO_PTG1, NULL);
	gpio_request(GPIO_PTG2, NULL);
	gpio_request(GPIO_PTG3, NULL);
	gpio_direction_output(GPIO_PTG0, 0);
	gpio_direction_output(GPIO_PTG1, 0);
	gpio_direction_output(GPIO_PTG2, 0);
	gpio_direction_output(GPIO_PTG3, 0);

	/* enable SH-Eth */
	gpio_request(GPIO_PTA1, NULL);
	gpio_direction_output(GPIO_PTA1, 1);
	mdelay(20);

	gpio_request(GPIO_FN_RMII_RXD0,    NULL);
	gpio_request(GPIO_FN_RMII_RXD1,    NULL);
	gpio_request(GPIO_FN_RMII_TXD0,    NULL);
	gpio_request(GPIO_FN_RMII_TXD1,    NULL);
	gpio_request(GPIO_FN_RMII_REF_CLK, NULL);
	gpio_request(GPIO_FN_RMII_TX_EN,   NULL);
	gpio_request(GPIO_FN_RMII_RX_ER,   NULL);
	gpio_request(GPIO_FN_RMII_CRS_DV,  NULL);
	gpio_request(GPIO_FN_MDIO,         NULL);
	gpio_request(GPIO_FN_MDC,          NULL);
	gpio_request(GPIO_FN_LNKSTA,       NULL);

	/* enable USB */
	gpio_request(GPIO_PTB3,  NULL);
	gpio_request(GPIO_PTB4,  NULL);
	gpio_request(GPIO_PTB5,  NULL);
	gpio_direction_input(GPIO_PTB3);
	gpio_direction_output(GPIO_PTB4, 0);
	gpio_direction_output(GPIO_PTB5, 0);
	ctrl_outw(0x0600, 0xa40501d4);
	ctrl_outw(0x0600, 0xa4050192);

	return platform_add_devices(ecovec_devices,
				    ARRAY_SIZE(ecovec_devices));
}
device_initcall(devices_setup);

static struct sh_machine_vector mv_ecovec __initmv = {
	.mv_name	= "R0P7724 (EcoVec)",
};
