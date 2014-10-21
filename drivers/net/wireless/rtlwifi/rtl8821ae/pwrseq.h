/******************************************************************************
 *
 * Copyright(c) 2009-2010  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __RTL8821AE_PWRSEQ_H__
#define __RTL8821AE_PWRSEQ_H__

#include "../pwrseqcmd.h"
#include "../btcoexist/halbt_precomp.h"

#define	RTL8812_TRANS_CARDEMU_TO_ACT_STEPS	15
#define	RTL8812_TRANS_ACT_TO_CARDEMU_STEPS	15
#define	RTL8812_TRANS_CARDEMU_TO_SUS_STEPS	15
#define	RTL8812_TRANS_SUS_TO_CARDEMU_STEPS	15
#define	RTL8812_TRANS_CARDEMU_TO_PDN_STEPS	25
#define	RTL8812_TRANS_PDN_TO_CARDEMU_STEPS	15
#define	RTL8812_TRANS_ACT_TO_LPS_STEPS		15
#define	RTL8812_TRANS_LPS_TO_ACT_STEPS		15
#define	RTL8812_TRANS_END_STEPS			1

/* The following macros have the following format:
 * { offset, cut_msk, fab_msk|interface_msk, base|cmd, msk, value
 *   comments },
 */
#define RTL8812_TRANS_CARDEMU_TO_ACT					\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT2, 0 \
	/* disable SW LPS 0x04[10]=0*/},	\
	{0x0006, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT1, BIT1 \
	/* wait till 0x04[17] = 1    power ready*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT7, 0 \
	/* disable HWPDN 0x04[15]=0*/}, \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3, 0 \
	/* disable WL suspend*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	/* polling until return 0*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT0, 0},

#define RTL8812_TRANS_ACT_TO_CARDEMU													\
	{0x0c00, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x04 \
	 /* 0xc00[7:0] = 4	turn off 3-wire */},	\
	{0x0e00, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x04 \
	 /* 0xe00[7:0] = 4	turn off 3-wire */},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	 /* 0x2[0] = 0	 RESET BB, CLOSE RF */},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_DELAY, 0, PWRSEQ_DELAY_US \
	/*Delay 1us*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	  /* Whole BB is reset*/},			\
	{0x0007, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x2A \
	 /* 0x07[7:0] = 0x28 sps pwm mode 0x2a for BT coex*/},	\
	{0x0008, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x02, 0 \
	/*0x8[1] = 0 ANA clk =500k */},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	 /*0x04[9] = 1 turn off MAC by HW state machine*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT1, 0 \
	 /*wait till 0x04[9] = 0 polling until return 0 to disable*/},

#define RTL8812_TRANS_CARDEMU_TO_SUS					\
	{0x0042, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xF0, 0xcc}, \
	{0x0042, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xF0, 0xEC}, \
	{0x0043, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x07 \
	/* gpio11 input mode, gpio10~8 output mode */},	\
	{0x0045, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/* gpio 0~7 output same value as input ?? */},	\
	{0x0046, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xff \
	/* gpio0~7 output mode */},	\
	{0x0047, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	/* 0x47[7:0] = 00 gpio mode */},	\
	{0x0007, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	/* suspend option all off */},	\
	{0x0014, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x80, BIT7 \
	/*0x14[7] = 1 turn on ZCD */},	\
	{0x0015, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x01, BIT0 \
	/* 0x15[0] =1 trun on ZCD */},	\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x10, BIT4 \
	/*0x23[4] = 1 hpon LDO sleep mode */},	\
	{0x0008, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x02, 0 \
	/*0x8[1] = 0 ANA clk =500k */},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3, BIT3 \
	/*0x04[11] = 2b'11 enable WL suspend for PCIe*/},

#define RTL8812_TRANS_SUS_TO_CARDEMU					\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3, 0 \
	/*0x04[11] = 2b'01enable WL suspend*/},   \
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x10, 0 \
	/*0x23[4] = 0 hpon LDO sleep mode leave */},	\
	{0x0015, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x01, 0 \
	/* 0x15[0] =0 trun off ZCD */},	\
	{0x0014, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x80, 0 \
	/*0x14[7] = 0 turn off ZCD */},	\
	{0x0046, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/* gpio0~7 input mode */},	\
	{0x0043, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/* gpio11 input mode, gpio10~8 input mode */},

#define RTL8812_TRANS_CARDEMU_TO_CARDDIS				\
	{0x0003, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT2, 0 \
	/*0x03[2] = 0, reset 8051*/},	\
	{0x0080, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x05 \
	/*0x80=05h if reload fw, fill the default value of host_CPU handshake field*/},	\
	{0x0042, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xF0, 0xcc}, \
	{0x0042, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xF0, 0xEC}, \
	{0x0043, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x07 \
	/* gpio11 input mode, gpio10~8 output mode */},	\
	{0x0045, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/* gpio 0~7 output same value as input ?? */},	\
	{0x0046, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xff \
	/* gpio0~7 output mode */},	\
	{0x0047, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	/* 0x47[7:0] = 00 gpio mode */},	\
	{0x0014, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x80, BIT7 \
	/*0x14[7] = 1 turn on ZCD */},	\
	{0x0015, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x01, BIT0 \
	/* 0x15[0] =1 trun on ZCD */},	\
	{0x0012, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x01, 0 \
	/*0x12[0] = 0 force PFM mode */},	\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x10, BIT4 \
	/*0x23[4] = 1 hpon LDO sleep mode */},	\
	{0x0008, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x02, 0 \
	/*0x8[1] = 0 ANA clk =500k */},	\
	{0x0007, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x20 \
	 /*0x07=0x20 , SOP option to disable BG/MB*/},	\
	{0x001f, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	 /*0x01f[1]=0 , disable RFC_0  control  REG_RF_CTRL_8812 */},	\
	{0x0076, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	 /*0x076[1]=0 , disable RFC_1  control REG_OPT_CTRL_8812 +2 */},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3, BIT3 \
	 /*0x04[11] = 2b'01 enable WL suspend*/},

#define RTL8812_TRANS_CARDDIS_TO_CARDEMU				\
	{0x0012, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	/*0x12[0] = 1 force PWM mode */},	\
	{0x0014, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x80, 0 \
	/*0x14[7] = 0 turn off ZCD */},	\
	{0x0015, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x01, 0 \
	/* 0x15[0] =0 trun off ZCD */},	\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0x10, 0 \
	/*0x23[4] = 0 hpon LDO leave sleep mode */},	\
	{0x0046, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/* gpio0~7 input mode */},	\
	{0x0043, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/* gpio11 input mode, gpio10~8 input mode */}, \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT2, 0 \
	 /*0x04[10] = 0, enable SW LPS PCIE only*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3, 0 \
	 /*0x04[11] = 2b'01enable WL suspend*/},	\
	{0x0003, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT2, BIT2 \
	 /*0x03[2] = 1, enable 8051*/},	\
	{0x0301, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	/*PCIe DMA start*/},

#define RTL8812_TRANS_CARDEMU_TO_PDN		\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT7, BIT7 \
	/* 0x04[15] = 1*/},

#define RTL8812_TRANS_PDN_TO_CARDEMU			\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT7, 0 \
	/* 0x04[15] = 0*/},

#define RTL8812_TRANS_ACT_TO_LPS		\
	{0x0301, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xFF \
	/*PCIe DMA stop*/},	\
	{0x0522, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x7F \
	/*Tx Pause*/},		\
	{0x05F8, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x05F9, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x05FA, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x05FB, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x0c00, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x04 \
	 /* 0xc00[7:0] = 4	turn off 3-wire */},	\
	{0x0e00, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x04 \
	 /* 0xe00[7:0] = 4	turn off 3-wire */},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	/*CCK and OFDM are disabled,and clock are gated,and RF closed*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_DELAY, 0, PWRSEQ_DELAY_US \
	/*Delay 1us*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	  /* Whole BB is reset*/},			\
	{0x0100, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x03 \
	/*Reset MAC TRX*/},			\
	{0x0101, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	/*check if removed later*/},		\
	{0x0553, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT5, BIT5 \
	/*Respond TxOK to scheduler*/},

#define RTL8812_TRANS_LPS_TO_ACT					\
	{0x0080, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_WRITE, 0xFF, 0x84 \
	 /*SDIO RPWM*/},	\
	{0xFE58, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x84 \
	 /*USB RPWM*/},	\
	{0x0361, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x84 \
	 /*PCIe RPWM*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_DELAY, 0, PWRSEQ_DELAY_MS \
	 /*Delay*/},	\
	{0x0008, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, 0 \
	 /*.	0x08[4] = 0		 switch TSF to 40M*/},	\
	{0x0109, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT7, 0 \
	 /*Polling 0x109[7]=0  TSF in 40M*/},			\
	{0x0029, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT6|BIT7, 0 \
	 /*.	0x29[7:6] = 2b'00	 enable BB clock*/},	\
	{0x0101, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	 /*.	0x101[1] = 1*/},					\
	{0x0100, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xFF \
	 /*.	0x100[7:0] = 0xFF	 enable WMAC TRX*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1|BIT0, BIT1|BIT0 \
	 /*.	0x02[1:0] = 2b'11	 enable BB macro*/},	\
	{0x0522, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	 /*.	0x522 = 0*/},

#define RTL8812_TRANS_END					\
	{0xFFFF, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK, \
	0, PWR_CMD_END, 0, 0},

extern struct wlan_pwr_cfg  rtl8812_power_on_flow
		[RTL8812_TRANS_CARDEMU_TO_ACT_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_radio_off_flow
		[RTL8812_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_card_disable_flow
		[RTL8812_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8812_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_card_enable_flow
		[RTL8812_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8812_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_suspend_flow
		[RTL8812_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8812_TRANS_CARDEMU_TO_SUS_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_resume_flow
		[RTL8812_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8812_TRANS_CARDEMU_TO_SUS_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_hwpdn_flow
		[RTL8812_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8812_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_enter_lps_flow
		[RTL8812_TRANS_ACT_TO_LPS_STEPS +
		 RTL8812_TRANS_END_STEPS];
extern struct wlan_pwr_cfg  rtl8812_leave_lps_flow
		[RTL8812_TRANS_LPS_TO_ACT_STEPS +
		 RTL8812_TRANS_END_STEPS];

/* Check document WM-20130516-JackieLau-RTL8821A_Power_Architecture-R10.vsd
 *	There are 6 HW Power States:
 *	0: POFF--Power Off
 *	1: PDN--Power Down
 *	2: CARDEMU--Card Emulation
 *	3: ACT--Active Mode
 *	4: LPS--Low Power State
 *	5: SUS--Suspend
 *
 *	The transision from different states are defined below
 *	TRANS_CARDEMU_TO_ACT
 *	TRANS_ACT_TO_CARDEMU
 *	TRANS_CARDEMU_TO_SUS
 *	TRANS_SUS_TO_CARDEMU
 *	TRANS_CARDEMU_TO_PDN
 *	TRANS_ACT_TO_LPS
 *	TRANS_LPS_TO_ACT
 *
 *	TRANS_END
 */
#define	RTL8821A_TRANS_CARDEMU_TO_ACT_STEPS	25
#define	RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS	15
#define	RTL8821A_TRANS_CARDEMU_TO_SUS_STEPS	15
#define	RTL8821A_TRANS_SUS_TO_CARDEMU_STEPS	15
#define RTL8821A_TRANS_CARDDIS_TO_CARDEMU_STEPS	15
#define	RTL8821A_TRANS_CARDEMU_TO_PDN_STEPS	15
#define	RTL8821A_TRANS_PDN_TO_CARDEMU_STEPS	15
#define	RTL8821A_TRANS_ACT_TO_LPS_STEPS		15
#define	RTL8821A_TRANS_LPS_TO_ACT_STEPS		15
#define	RTL8821A_TRANS_END_STEPS		1

#define RTL8821A_TRANS_CARDEMU_TO_ACT					\
	{0x0020, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,			\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	 /*0x20[0] = 1b'1 enable LDOA12 MACRO block for all interface*/},   \
	{0x0067, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,		\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, 0 \
	 /*0x67[0] = 0 to disable BT_GPS_SEL pins*/},	\
	{0x0001, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,			\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_DELAY, 1, PWRSEQ_DELAY_MS \
	/*Delay 1ms*/},   \
	{0x0000, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,			\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT5, 0 \
	 /*0x00[5] = 1b'0 release analog Ips to digital ,1:isolation*/},   \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, (BIT4|BIT3|BIT2), 0 \
	/* disable SW LPS 0x04[10]=0 and WLSUS_EN 0x04[12:11]=0*/},	\
	{0x0075, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0 , BIT0 \
	/* Disable USB suspend */},	\
	{0x0006, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT1, BIT1 \
	/* wait till 0x04[17] = 1    power ready*/},	\
	{0x0075, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0 , 0 \
	/* Enable USB suspend */},	\
	{0x0006, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	/* release WLON reset  0x04[16]=1*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT7, 0 \
	/* disable HWPDN 0x04[15]=0*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, (BIT4|BIT3), 0 \
	/* disable WL suspend*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	/* polling until return 0*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT0, 0 \
	/**/},	\
	{0x004F, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	/*0x4C[24] = 0x4F[0] = 1, switch DPDT_SEL_P output from WL BB */},\
	{0x0067, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, (BIT5|BIT4), (BIT5|BIT4) \
	/*0x66[13] = 0x67[5] = 1, switch for PAPE_G/PAPE_A 	\
	 from WL BB ; 0x66[12] = 0x67[4] = 1, switch LNAON from WL BB */},\
	{0x0025, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT6, 0 \
	/*anapar_mac<118> , 0x25[6]=0 by wlan single function*/},\
	{0x0049, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	/*Enable falling edge triggering interrupt*/},\
	{0x0063, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	/*Enable GPIO9 interrupt mode*/},\
	{0x0062, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	/*Enable GPIO9 input mode*/},\
	{0x0058, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, BIT0 \
	/*Enable HSISR GPIO[C:0] interrupt*/},\
	{0x005A, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	/*Enable HSISR GPIO9 interrupt*/},\
	{0x007A, PWR_CUT_TESTCHIP_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x3A \
	/*0x7A = 0x3A start BT*/},\
	{0x002E, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF , 0x82  \
	/* 0x2C[23:12]=0x820 ; XTAL trim */}, \
	{0x0010, PWR_CUT_A_MSK , PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT6 , BIT6  \
	/* 0x10[6]=1  */},

#define RTL8821A_TRANS_ACT_TO_CARDEMU					\
	{0x001F, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	/*0x1F[7:0] = 0 turn off RF*/},	\
	{0x004F, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	/*0x4C[24] = 0x4F[0] = 0, switch DPDT_SEL_P output from		\
	 register 0x65[2] */},\
	{0x0049, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	/*Enable rising edge triggering interrupt*/}, \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	 /*0x04[9] = 1 turn off MAC by HW state machine*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT1, 0 \
	 /*wait till 0x04[9] = 0 polling until return 0 to disable*/},	\
	{0x0000, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,			\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT5, BIT5 \
	 /*0x00[5] = 1b'1 analog Ips to digital ,1:isolation*/},   \
	{0x0020, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,		\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	 /*0x20[0] = 1b'0 disable LDOA12 MACRO block*/},

#define RTL8821A_TRANS_CARDEMU_TO_SUS					\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4|BIT3, (BIT4|BIT3) \
	 /*0x04[12:11] = 2b'11 enable WL suspend for PCIe*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,		\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3|BIT4, BIT3 \
	 /*0x04[12:11] = 2b'01 enable WL suspend*/},	\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, BIT4 \
	 /*0x23[4] = 1b'1 12H LDO enter sleep mode*/},   \
	{0x0007, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x20 \
	 /*0x07[7:0] = 0x20 SDIO SOP option to disable BG/MB/ACK/SWR*/},   \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3|BIT4, BIT3|BIT4 \
	 /*0x04[12:11] = 2b'11 enable WL suspend for PCIe*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_WRITE, BIT0, BIT0 \
	 /*Set SDIO suspend local register*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_POLLING, BIT1, 0 \
	 /*wait power state to suspend*/},

#define RTL8821A_TRANS_SUS_TO_CARDEMU					\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3 | BIT7, 0 \
	 /*clear suspend enable and power down enable*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_WRITE, BIT0, 0 \
	 /*Set SDIO suspend local register*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_POLLING, BIT1, BIT1 \
	 /*wait power state to suspend*/},\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, 0 \
	 /*0x23[4] = 1b'0 12H LDO enter normal mode*/},   \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3|BIT4, 0 \
	 /*0x04[12:11] = 2b'01enable WL suspend*/},

#define RTL8821A_TRANS_CARDEMU_TO_CARDDIS				\
	{0x0007, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x20 \
	 /*0x07=0x20 , SOP option to disable BG/MB*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,		\
	 PWR_INTF_USB_MSK|PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3|BIT4, BIT3 \
	 /*0x04[12:11] = 2b'01 enable WL suspend*/},	\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT2, BIT2 \
	 /*0x04[10] = 1, enable SW LPS*/},	\
        {0x004A, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 1 \
	 /*0x48[16] = 1 to enable GPIO9 as EXT WAKEUP*/},   \
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, BIT4 \
	 /*0x23[4] = 1b'1 12H LDO enter sleep mode*/},   \
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_WRITE, BIT0, BIT0 \
	 /*Set SDIO suspend local register*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_POLLING, BIT1, 0 \
	 /*wait power state to suspend*/},

#define RTL8821A_TRANS_CARDDIS_TO_CARDEMU				\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3 | BIT7, 0 \
	 /*clear suspend enable and power down enable*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_WRITE, BIT0, 0 \
	 /*Set SDIO suspend local register*/},	\
	{0x0086, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_POLLING, BIT1, BIT1 \
	 /*wait power state to suspend*/},\
	{0x004A, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	 /*0x48[16] = 0 to disable GPIO9 as EXT WAKEUP*/},   \
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT3|BIT4, 0 \
	 /*0x04[12:11] = 2b'01enable WL suspend*/},\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, 0 \
	 /*0x23[4] = 1b'0 12H LDO enter normal mode*/},   \
	{0x0301, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	/*PCIe DMA start*/},

#define RTL8821A_TRANS_CARDEMU_TO_PDN					\
	{0x0023, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, BIT4 \
	 /*0x23[4] = 1b'1 12H LDO enter sleep mode*/},   \
	{0x0007, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,		\
	 PWR_INTF_SDIO_MSK|PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x20 \
	 /*0x07[7:0] = 0x20 SOP option to disable BG/MB/ACK/SWR*/},   \
	{0x0006, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	/* 0x04[16] = 0*/},\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT7, BIT7 \
	/* 0x04[15] = 1*/},

#define RTL8821A_TRANS_PDN_TO_CARDEMU				\
	{0x0005, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT7, 0 \
	/* 0x04[15] = 0*/},

#define RTL8821A_TRANS_ACT_TO_LPS					\
	{0x0301, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xFF \
	/*PCIe DMA stop*/},	\
	{0x0522, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xFF \
	/*Tx Pause*/},	\
	{0x05F8, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x05F9, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x05FA, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x05FB, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, 0xFF, 0 \
	/*Should be zero if no packet is transmitting*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT0, 0 \
	/*CCK and OFDM are disabled,and clock are gated*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_DELAY, 0, PWRSEQ_DELAY_US \
	/*Delay 1us*/},	\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	/*Whole BB is reset*/},	\
	{0x0100, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x03 \
	/*Reset MAC TRX*/},	\
	{0x0101, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, 0 \
	/*check if removed later*/},	\
	{0x0093, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x00 \
	/*When driver enter Sus/ Disable, enable LOP for BT*/},	\
	{0x0553, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT5, BIT5 \
	/*Respond TxOK to scheduler*/},

#define RTL8821A_TRANS_LPS_TO_ACT					\
	{0x0080, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_SDIO_MSK,\
	PWR_BASEADDR_SDIO, PWR_CMD_WRITE, 0xFF, 0x84 \
	 /*SDIO RPWM*/},\
	{0xFE58, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_USB_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x84 \
	 /*USB RPWM*/},\
	{0x0361, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0x84 \
	 /*PCIe RPWM*/},\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_DELAY, 0, PWRSEQ_DELAY_MS \
	 /*Delay*/},\
	{0x0008, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT4, 0 \
	 /*.	0x08[4] = 0		 switch TSF to 40M*/},\
	{0x0109, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_POLLING, BIT7, 0 \
	 /*Polling 0x109[7]=0  TSF in 40M*/},\
	{0x0029, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT6|BIT7, 0 \
	 /*.	0x29[7:6] = 2b'00	 enable BB clock*/},\
	{0x0101, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1, BIT1 \
	 /*.	0x101[1] = 1*/},\
	{0x0100, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0xFF \
	 /*.	0x100[7:0] = 0xFF	 enable WMAC TRX*/},\
	{0x0002, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, BIT1|BIT0, BIT1|BIT0 \
	 /*.	0x02[1:0] = 2b'11	 enable BB macro*/},\
	{0x0522, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	PWR_BASEADDR_MAC, PWR_CMD_WRITE, 0xFF, 0 \
	 /*.	0x522 = 0*/},

#define RTL8821A_TRANS_END					\
	{0xFFFF, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_ALL_MSK,\
	0, PWR_CMD_END, 0, 0},

extern struct wlan_pwr_cfg rtl8821A_power_on_flow
		[RTL8821A_TRANS_CARDEMU_TO_ACT_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_radio_off_flow
		[RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_card_disable_flow
		[RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8821A_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_card_enable_flow
		[RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8821A_TRANS_CARDEMU_TO_ACT_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_suspend_flow
		[RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8821A_TRANS_CARDEMU_TO_SUS_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_resume_flow
		[RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8821A_TRANS_CARDEMU_TO_SUS_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_hwpdn_flow
		[RTL8821A_TRANS_ACT_TO_CARDEMU_STEPS +
		 RTL8821A_TRANS_CARDEMU_TO_PDN_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_enter_lps_flow
		[RTL8821A_TRANS_ACT_TO_LPS_STEPS +
		 RTL8821A_TRANS_END_STEPS];
extern struct wlan_pwr_cfg rtl8821A_leave_lps_flow
		[RTL8821A_TRANS_LPS_TO_ACT_STEPS +
		 RTL8821A_TRANS_END_STEPS];

/*RTL8812 Power Configuration CMDs for PCIe interface*/
#define RTL8812_NIC_PWR_ON_FLOW			rtl8812_power_on_flow
#define RTL8812_NIC_RF_OFF_FLOW			rtl8812_radio_off_flow
#define RTL8812_NIC_DISABLE_FLOW		rtl8812_card_disable_flow
#define RTL8812_NIC_ENABLE_FLOW			rtl8812_card_enable_flow
#define RTL8812_NIC_SUSPEND_FLOW		rtl8812_suspend_flow
#define RTL8812_NIC_RESUME_FLOW			rtl8812_resume_flow
#define RTL8812_NIC_PDN_FLOW			rtl8812_hwpdn_flow
#define RTL8812_NIC_LPS_ENTER_FLOW		rtl8812_enter_lps_flow
#define RTL8812_NIC_LPS_LEAVE_FLOW		rtl8812_leave_lps_flow

/* RTL8821 Power Configuration CMDs for PCIe interface */
#define RTL8821A_NIC_PWR_ON_FLOW		rtl8821A_power_on_flow
#define RTL8821A_NIC_RF_OFF_FLOW		rtl8821A_radio_off_flow
#define RTL8821A_NIC_DISABLE_FLOW		rtl8821A_card_disable_flow
#define RTL8821A_NIC_ENABLE_FLOW		rtl8821A_card_enable_flow
#define RTL8821A_NIC_SUSPEND_FLOW		rtl8821A_suspend_flow
#define RTL8821A_NIC_RESUME_FLOW		rtl8821A_resume_flow
#define RTL8821A_NIC_PDN_FLOW			rtl8821A_hwpdn_flow
#define RTL8821A_NIC_LPS_ENTER_FLOW		rtl8821A_enter_lps_flow
#define RTL8821A_NIC_LPS_LEAVE_FLOW		rtl8821A_leave_lps_flow

#endif
