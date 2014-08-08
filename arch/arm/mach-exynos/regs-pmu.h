/*
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power management unit definition
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_REGS_PMU_H
#define __ASM_ARCH_REGS_PMU_H __FILE__

#include <mach/map.h>

#define S5P_PMUREG(x)				(S5P_VA_PMU + (x))

#define S5P_CENTRAL_SEQ_CONFIGURATION		S5P_PMUREG(0x0200)

#define S5P_CENTRAL_LOWPWR_CFG			(1 << 16)

#define S5P_CENTRAL_SEQ_OPTION			S5P_PMUREG(0x0208)

#define S5P_USE_STANDBY_WFI0			(1 << 16)
#define S5P_USE_STANDBY_WFE0			(1 << 24)

#define EXYNOS_SWRESET				S5P_PMUREG(0x0400)
#define EXYNOS5440_SWRESET			S5P_PMUREG(0x00C4)

#define S5P_WAKEUP_STAT				S5P_PMUREG(0x0600)
#define S5P_EINT_WAKEUP_MASK			S5P_PMUREG(0x0604)
#define S5P_WAKEUP_MASK				S5P_PMUREG(0x0608)

#define S5P_INFORM0				S5P_PMUREG(0x0800)
#define S5P_INFORM1				S5P_PMUREG(0x0804)
#define S5P_INFORM5				S5P_PMUREG(0x0814)
#define S5P_INFORM6				S5P_PMUREG(0x0818)
#define S5P_INFORM7				S5P_PMUREG(0x081C)
#define S5P_PMU_SPARE3				S5P_PMUREG(0x090C)

#define S5P_ARM_CORE0_LOWPWR			S5P_PMUREG(0x1000)
#define S5P_DIS_IRQ_CORE0			S5P_PMUREG(0x1004)
#define S5P_DIS_IRQ_CENTRAL0			S5P_PMUREG(0x1008)
#define S5P_ARM_CORE1_LOWPWR			S5P_PMUREG(0x1010)
#define S5P_DIS_IRQ_CORE1			S5P_PMUREG(0x1014)
#define S5P_DIS_IRQ_CENTRAL1			S5P_PMUREG(0x1018)
#define S5P_ARM_COMMON_LOWPWR			S5P_PMUREG(0x1080)
#define S5P_L2_0_LOWPWR				S5P_PMUREG(0x10C0)
#define S5P_L2_1_LOWPWR				S5P_PMUREG(0x10C4)
#define S5P_CMU_ACLKSTOP_LOWPWR			S5P_PMUREG(0x1100)
#define S5P_CMU_SCLKSTOP_LOWPWR			S5P_PMUREG(0x1104)
#define S5P_CMU_RESET_LOWPWR			S5P_PMUREG(0x110C)
#define S5P_APLL_SYSCLK_LOWPWR			S5P_PMUREG(0x1120)
#define S5P_MPLL_SYSCLK_LOWPWR			S5P_PMUREG(0x1124)
#define S5P_VPLL_SYSCLK_LOWPWR			S5P_PMUREG(0x1128)
#define S5P_EPLL_SYSCLK_LOWPWR			S5P_PMUREG(0x112C)
#define S5P_CMU_CLKSTOP_GPS_ALIVE_LOWPWR	S5P_PMUREG(0x1138)
#define S5P_CMU_RESET_GPSALIVE_LOWPWR		S5P_PMUREG(0x113C)
#define S5P_CMU_CLKSTOP_CAM_LOWPWR		S5P_PMUREG(0x1140)
#define S5P_CMU_CLKSTOP_TV_LOWPWR		S5P_PMUREG(0x1144)
#define S5P_CMU_CLKSTOP_MFC_LOWPWR		S5P_PMUREG(0x1148)
#define S5P_CMU_CLKSTOP_G3D_LOWPWR		S5P_PMUREG(0x114C)
#define S5P_CMU_CLKSTOP_LCD0_LOWPWR		S5P_PMUREG(0x1150)
#define S5P_CMU_CLKSTOP_MAUDIO_LOWPWR		S5P_PMUREG(0x1158)
#define S5P_CMU_CLKSTOP_GPS_LOWPWR		S5P_PMUREG(0x115C)
#define S5P_CMU_RESET_CAM_LOWPWR		S5P_PMUREG(0x1160)
#define S5P_CMU_RESET_TV_LOWPWR			S5P_PMUREG(0x1164)
#define S5P_CMU_RESET_MFC_LOWPWR		S5P_PMUREG(0x1168)
#define S5P_CMU_RESET_G3D_LOWPWR		S5P_PMUREG(0x116C)
#define S5P_CMU_RESET_LCD0_LOWPWR		S5P_PMUREG(0x1170)
#define S5P_CMU_RESET_MAUDIO_LOWPWR		S5P_PMUREG(0x1178)
#define S5P_CMU_RESET_GPS_LOWPWR		S5P_PMUREG(0x117C)
#define S5P_TOP_BUS_LOWPWR			S5P_PMUREG(0x1180)
#define S5P_TOP_RETENTION_LOWPWR		S5P_PMUREG(0x1184)
#define S5P_TOP_PWR_LOWPWR			S5P_PMUREG(0x1188)
#define S5P_LOGIC_RESET_LOWPWR			S5P_PMUREG(0x11A0)
#define S5P_ONENAND_MEM_LOWPWR			S5P_PMUREG(0x11C0)
#define S5P_G2D_ACP_MEM_LOWPWR			S5P_PMUREG(0x11C8)
#define S5P_USBOTG_MEM_LOWPWR			S5P_PMUREG(0x11CC)
#define S5P_HSMMC_MEM_LOWPWR			S5P_PMUREG(0x11D0)
#define S5P_CSSYS_MEM_LOWPWR			S5P_PMUREG(0x11D4)
#define S5P_SECSS_MEM_LOWPWR			S5P_PMUREG(0x11D8)
#define S5P_PAD_RETENTION_DRAM_LOWPWR		S5P_PMUREG(0x1200)
#define S5P_PAD_RETENTION_MAUDIO_LOWPWR		S5P_PMUREG(0x1204)
#define S5P_PAD_RETENTION_GPIO_LOWPWR		S5P_PMUREG(0x1220)
#define S5P_PAD_RETENTION_UART_LOWPWR		S5P_PMUREG(0x1224)
#define S5P_PAD_RETENTION_MMCA_LOWPWR		S5P_PMUREG(0x1228)
#define S5P_PAD_RETENTION_MMCB_LOWPWR		S5P_PMUREG(0x122C)
#define S5P_PAD_RETENTION_EBIA_LOWPWR		S5P_PMUREG(0x1230)
#define S5P_PAD_RETENTION_EBIB_LOWPWR		S5P_PMUREG(0x1234)
#define S5P_PAD_RETENTION_ISOLATION_LOWPWR	S5P_PMUREG(0x1240)
#define S5P_PAD_RETENTION_ALV_SEL_LOWPWR	S5P_PMUREG(0x1260)
#define S5P_XUSBXTI_LOWPWR			S5P_PMUREG(0x1280)
#define S5P_XXTI_LOWPWR				S5P_PMUREG(0x1284)
#define S5P_EXT_REGULATOR_LOWPWR		S5P_PMUREG(0x12C0)
#define S5P_GPIO_MODE_LOWPWR			S5P_PMUREG(0x1300)
#define S5P_GPIO_MODE_MAUDIO_LOWPWR		S5P_PMUREG(0x1340)
#define S5P_CAM_LOWPWR				S5P_PMUREG(0x1380)
#define S5P_TV_LOWPWR				S5P_PMUREG(0x1384)
#define S5P_MFC_LOWPWR				S5P_PMUREG(0x1388)
#define S5P_G3D_LOWPWR				S5P_PMUREG(0x138C)
#define S5P_LCD0_LOWPWR				S5P_PMUREG(0x1390)
#define S5P_MAUDIO_LOWPWR			S5P_PMUREG(0x1398)
#define S5P_GPS_LOWPWR				S5P_PMUREG(0x139C)
#define S5P_GPS_ALIVE_LOWPWR			S5P_PMUREG(0x13A0)

#define EXYNOS_ARM_CORE0_CONFIGURATION		S5P_PMUREG(0x2000)
#define EXYNOS_ARM_CORE_CONFIGURATION(_nr)	\
			(EXYNOS_ARM_CORE0_CONFIGURATION + (0x80 * (_nr)))
#define EXYNOS_ARM_CORE_STATUS(_nr)		\
			(EXYNOS_ARM_CORE_CONFIGURATION(_nr) + 0x4)

#define EXYNOS_ARM_COMMON_CONFIGURATION		S5P_PMUREG(0x2500)
#define EXYNOS_COMMON_CONFIGURATION(_nr)	\
			(EXYNOS_ARM_COMMON_CONFIGURATION + (0x80 * (_nr)))
#define EXYNOS_COMMON_STATUS(_nr)		\
			(EXYNOS_COMMON_CONFIGURATION(_nr) + 0x4)

#define S5P_PAD_RET_MAUDIO_OPTION		S5P_PMUREG(0x3028)
#define S5P_PAD_RET_GPIO_OPTION			S5P_PMUREG(0x3108)
#define S5P_PAD_RET_UART_OPTION			S5P_PMUREG(0x3128)
#define S5P_PAD_RET_MMCA_OPTION			S5P_PMUREG(0x3148)
#define S5P_PAD_RET_MMCB_OPTION			S5P_PMUREG(0x3168)
#define S5P_PAD_RET_EBIA_OPTION			S5P_PMUREG(0x3188)
#define S5P_PAD_RET_EBIB_OPTION			S5P_PMUREG(0x31A8)

#define S5P_CORE_LOCAL_PWR_EN			0x3

/* Only for EXYNOS4210 */
#define S5P_CMU_CLKSTOP_LCD1_LOWPWR	S5P_PMUREG(0x1154)
#define S5P_CMU_RESET_LCD1_LOWPWR	S5P_PMUREG(0x1174)
#define S5P_MODIMIF_MEM_LOWPWR		S5P_PMUREG(0x11C4)
#define S5P_PCIE_MEM_LOWPWR		S5P_PMUREG(0x11E0)
#define S5P_SATA_MEM_LOWPWR		S5P_PMUREG(0x11E4)
#define S5P_LCD1_LOWPWR			S5P_PMUREG(0x1394)

/* Only for EXYNOS4x12 */
#define S5P_ISP_ARM_LOWPWR			S5P_PMUREG(0x1050)
#define S5P_DIS_IRQ_ISP_ARM_LOCAL_LOWPWR	S5P_PMUREG(0x1054)
#define S5P_DIS_IRQ_ISP_ARM_CENTRAL_LOWPWR	S5P_PMUREG(0x1058)
#define S5P_CMU_ACLKSTOP_COREBLK_LOWPWR		S5P_PMUREG(0x1110)
#define S5P_CMU_SCLKSTOP_COREBLK_LOWPWR		S5P_PMUREG(0x1114)
#define S5P_CMU_RESET_COREBLK_LOWPWR		S5P_PMUREG(0x111C)
#define S5P_MPLLUSER_SYSCLK_LOWPWR		S5P_PMUREG(0x1130)
#define S5P_CMU_CLKSTOP_ISP_LOWPWR		S5P_PMUREG(0x1154)
#define S5P_CMU_RESET_ISP_LOWPWR		S5P_PMUREG(0x1174)
#define S5P_TOP_BUS_COREBLK_LOWPWR		S5P_PMUREG(0x1190)
#define S5P_TOP_RETENTION_COREBLK_LOWPWR	S5P_PMUREG(0x1194)
#define S5P_TOP_PWR_COREBLK_LOWPWR		S5P_PMUREG(0x1198)
#define S5P_OSCCLK_GATE_LOWPWR			S5P_PMUREG(0x11A4)
#define S5P_LOGIC_RESET_COREBLK_LOWPWR		S5P_PMUREG(0x11B0)
#define S5P_OSCCLK_GATE_COREBLK_LOWPWR		S5P_PMUREG(0x11B4)
#define S5P_HSI_MEM_LOWPWR			S5P_PMUREG(0x11C4)
#define S5P_ROTATOR_MEM_LOWPWR			S5P_PMUREG(0x11DC)
#define S5P_PAD_RETENTION_GPIO_COREBLK_LOWPWR	S5P_PMUREG(0x123C)
#define S5P_PAD_ISOLATION_COREBLK_LOWPWR	S5P_PMUREG(0x1250)
#define S5P_GPIO_MODE_COREBLK_LOWPWR		S5P_PMUREG(0x1320)
#define S5P_TOP_ASB_RESET_LOWPWR		S5P_PMUREG(0x1344)
#define S5P_TOP_ASB_ISOLATION_LOWPWR		S5P_PMUREG(0x1348)
#define S5P_ISP_LOWPWR				S5P_PMUREG(0x1394)
#define S5P_DRAM_FREQ_DOWN_LOWPWR		S5P_PMUREG(0x13B0)
#define S5P_DDRPHY_DLLOFF_LOWPWR		S5P_PMUREG(0x13B4)
#define S5P_CMU_SYSCLK_ISP_LOWPWR		S5P_PMUREG(0x13B8)
#define S5P_CMU_SYSCLK_GPS_LOWPWR		S5P_PMUREG(0x13BC)
#define S5P_LPDDR_PHY_DLL_LOCK_LOWPWR		S5P_PMUREG(0x13C0)

#define S5P_ARM_L2_0_OPTION			S5P_PMUREG(0x2608)
#define S5P_ARM_L2_1_OPTION			S5P_PMUREG(0x2628)
#define S5P_ONENAND_MEM_OPTION			S5P_PMUREG(0x2E08)
#define S5P_HSI_MEM_OPTION			S5P_PMUREG(0x2E28)
#define S5P_G2D_ACP_MEM_OPTION			S5P_PMUREG(0x2E48)
#define S5P_USBOTG_MEM_OPTION			S5P_PMUREG(0x2E68)
#define S5P_HSMMC_MEM_OPTION			S5P_PMUREG(0x2E88)
#define S5P_CSSYS_MEM_OPTION			S5P_PMUREG(0x2EA8)
#define S5P_SECSS_MEM_OPTION			S5P_PMUREG(0x2EC8)
#define S5P_ROTATOR_MEM_OPTION			S5P_PMUREG(0x2F48)

/* Only for EXYNOS4412 */
#define S5P_ARM_CORE2_LOWPWR			S5P_PMUREG(0x1020)
#define S5P_DIS_IRQ_CORE2			S5P_PMUREG(0x1024)
#define S5P_DIS_IRQ_CENTRAL2			S5P_PMUREG(0x1028)
#define S5P_ARM_CORE3_LOWPWR			S5P_PMUREG(0x1030)
#define S5P_DIS_IRQ_CORE3			S5P_PMUREG(0x1034)
#define S5P_DIS_IRQ_CENTRAL3			S5P_PMUREG(0x1038)

/* For EXYNOS5 */

#define EXYNOS5_AUTO_WDTRESET_DISABLE				S5P_PMUREG(0x0408)
#define EXYNOS5_MASK_WDTRESET_REQUEST				S5P_PMUREG(0x040C)

#define EXYNOS5_SYS_WDTRESET					(1 << 20)

#define EXYNOS5_ARM_CORE0_SYS_PWR_REG				S5P_PMUREG(0x1000)
#define EXYNOS5_DIS_IRQ_ARM_CORE0_LOCAL_SYS_PWR_REG		S5P_PMUREG(0x1004)
#define EXYNOS5_DIS_IRQ_ARM_CORE0_CENTRAL_SYS_PWR_REG		S5P_PMUREG(0x1008)
#define EXYNOS5_ARM_CORE1_SYS_PWR_REG				S5P_PMUREG(0x1010)
#define EXYNOS5_DIS_IRQ_ARM_CORE1_LOCAL_SYS_PWR_REG		S5P_PMUREG(0x1014)
#define EXYNOS5_DIS_IRQ_ARM_CORE1_CENTRAL_SYS_PWR_REG		S5P_PMUREG(0x1018)
#define EXYNOS5_FSYS_ARM_SYS_PWR_REG				S5P_PMUREG(0x1040)
#define EXYNOS5_DIS_IRQ_FSYS_ARM_CENTRAL_SYS_PWR_REG		S5P_PMUREG(0x1048)
#define EXYNOS5_ISP_ARM_SYS_PWR_REG				S5P_PMUREG(0x1050)
#define EXYNOS5_DIS_IRQ_ISP_ARM_LOCAL_SYS_PWR_REG		S5P_PMUREG(0x1054)
#define EXYNOS5_DIS_IRQ_ISP_ARM_CENTRAL_SYS_PWR_REG		S5P_PMUREG(0x1058)
#define EXYNOS5_ARM_COMMON_SYS_PWR_REG				S5P_PMUREG(0x1080)
#define EXYNOS5_ARM_L2_SYS_PWR_REG				S5P_PMUREG(0x10C0)
#define EXYNOS5_CMU_ACLKSTOP_SYS_PWR_REG			S5P_PMUREG(0x1100)
#define EXYNOS5_CMU_SCLKSTOP_SYS_PWR_REG			S5P_PMUREG(0x1104)
#define EXYNOS5_CMU_RESET_SYS_PWR_REG				S5P_PMUREG(0x110C)
#define EXYNOS5_CMU_ACLKSTOP_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x1120)
#define EXYNOS5_CMU_SCLKSTOP_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x1124)
#define EXYNOS5_CMU_RESET_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x112C)
#define EXYNOS5_DRAM_FREQ_DOWN_SYS_PWR_REG			S5P_PMUREG(0x1130)
#define EXYNOS5_DDRPHY_DLLOFF_SYS_PWR_REG			S5P_PMUREG(0x1134)
#define EXYNOS5_DDRPHY_DLLLOCK_SYS_PWR_REG			S5P_PMUREG(0x1138)
#define EXYNOS5_APLL_SYSCLK_SYS_PWR_REG				S5P_PMUREG(0x1140)
#define EXYNOS5_MPLL_SYSCLK_SYS_PWR_REG				S5P_PMUREG(0x1144)
#define EXYNOS5_VPLL_SYSCLK_SYS_PWR_REG				S5P_PMUREG(0x1148)
#define EXYNOS5_EPLL_SYSCLK_SYS_PWR_REG				S5P_PMUREG(0x114C)
#define EXYNOS5_BPLL_SYSCLK_SYS_PWR_REG				S5P_PMUREG(0x1150)
#define EXYNOS5_CPLL_SYSCLK_SYS_PWR_REG				S5P_PMUREG(0x1154)
#define EXYNOS5_MPLLUSER_SYSCLK_SYS_PWR_REG			S5P_PMUREG(0x1164)
#define EXYNOS5_BPLLUSER_SYSCLK_SYS_PWR_REG			S5P_PMUREG(0x1170)
#define EXYNOS5_TOP_BUS_SYS_PWR_REG				S5P_PMUREG(0x1180)
#define EXYNOS5_TOP_RETENTION_SYS_PWR_REG			S5P_PMUREG(0x1184)
#define EXYNOS5_TOP_PWR_SYS_PWR_REG				S5P_PMUREG(0x1188)
#define EXYNOS5_TOP_BUS_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x1190)
#define EXYNOS5_TOP_RETENTION_SYSMEM_SYS_PWR_REG		S5P_PMUREG(0x1194)
#define EXYNOS5_TOP_PWR_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x1198)
#define EXYNOS5_LOGIC_RESET_SYS_PWR_REG				S5P_PMUREG(0x11A0)
#define EXYNOS5_OSCCLK_GATE_SYS_PWR_REG				S5P_PMUREG(0x11A4)
#define EXYNOS5_LOGIC_RESET_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x11B0)
#define EXYNOS5_OSCCLK_GATE_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x11B4)
#define EXYNOS5_USBOTG_MEM_SYS_PWR_REG				S5P_PMUREG(0x11C0)
#define EXYNOS5_G2D_MEM_SYS_PWR_REG				S5P_PMUREG(0x11C8)
#define EXYNOS5_USBDRD_MEM_SYS_PWR_REG				S5P_PMUREG(0x11CC)
#define EXYNOS5_SDMMC_MEM_SYS_PWR_REG				S5P_PMUREG(0x11D0)
#define EXYNOS5_CSSYS_MEM_SYS_PWR_REG				S5P_PMUREG(0x11D4)
#define EXYNOS5_SECSS_MEM_SYS_PWR_REG				S5P_PMUREG(0x11D8)
#define EXYNOS5_ROTATOR_MEM_SYS_PWR_REG				S5P_PMUREG(0x11DC)
#define EXYNOS5_INTRAM_MEM_SYS_PWR_REG				S5P_PMUREG(0x11E0)
#define EXYNOS5_INTROM_MEM_SYS_PWR_REG				S5P_PMUREG(0x11E4)
#define EXYNOS5_JPEG_MEM_SYS_PWR_REG				S5P_PMUREG(0x11E8)
#define EXYNOS5_HSI_MEM_SYS_PWR_REG				S5P_PMUREG(0x11EC)
#define EXYNOS5_MCUIOP_MEM_SYS_PWR_REG				S5P_PMUREG(0x11F4)
#define EXYNOS5_SATA_MEM_SYS_PWR_REG				S5P_PMUREG(0x11FC)
#define EXYNOS5_PAD_RETENTION_DRAM_SYS_PWR_REG			S5P_PMUREG(0x1200)
#define EXYNOS5_PAD_RETENTION_MAU_SYS_PWR_REG			S5P_PMUREG(0x1204)
#define EXYNOS5_PAD_RETENTION_EFNAND_SYS_PWR_REG		S5P_PMUREG(0x1208)
#define EXYNOS5_PAD_RETENTION_GPIO_SYS_PWR_REG			S5P_PMUREG(0x1220)
#define EXYNOS5_PAD_RETENTION_UART_SYS_PWR_REG			S5P_PMUREG(0x1224)
#define EXYNOS5_PAD_RETENTION_MMCA_SYS_PWR_REG			S5P_PMUREG(0x1228)
#define EXYNOS5_PAD_RETENTION_MMCB_SYS_PWR_REG			S5P_PMUREG(0x122C)
#define EXYNOS5_PAD_RETENTION_EBIA_SYS_PWR_REG			S5P_PMUREG(0x1230)
#define EXYNOS5_PAD_RETENTION_EBIB_SYS_PWR_REG			S5P_PMUREG(0x1234)
#define EXYNOS5_PAD_RETENTION_SPI_SYS_PWR_REG			S5P_PMUREG(0x1238)
#define EXYNOS5_PAD_RETENTION_GPIO_SYSMEM_SYS_PWR_REG		S5P_PMUREG(0x123C)
#define EXYNOS5_PAD_ISOLATION_SYS_PWR_REG			S5P_PMUREG(0x1240)
#define EXYNOS5_PAD_ISOLATION_SYSMEM_SYS_PWR_REG		S5P_PMUREG(0x1250)
#define EXYNOS5_PAD_ALV_SEL_SYS_PWR_REG				S5P_PMUREG(0x1260)
#define EXYNOS5_XUSBXTI_SYS_PWR_REG				S5P_PMUREG(0x1280)
#define EXYNOS5_XXTI_SYS_PWR_REG				S5P_PMUREG(0x1284)
#define EXYNOS5_EXT_REGULATOR_SYS_PWR_REG			S5P_PMUREG(0x12C0)
#define EXYNOS5_GPIO_MODE_SYS_PWR_REG				S5P_PMUREG(0x1300)
#define EXYNOS5_GPIO_MODE_SYSMEM_SYS_PWR_REG			S5P_PMUREG(0x1320)
#define EXYNOS5_GPIO_MODE_MAU_SYS_PWR_REG			S5P_PMUREG(0x1340)
#define EXYNOS5_TOP_ASB_RESET_SYS_PWR_REG			S5P_PMUREG(0x1344)
#define EXYNOS5_TOP_ASB_ISOLATION_SYS_PWR_REG			S5P_PMUREG(0x1348)
#define EXYNOS5_GSCL_SYS_PWR_REG				S5P_PMUREG(0x1400)
#define EXYNOS5_ISP_SYS_PWR_REG					S5P_PMUREG(0x1404)
#define EXYNOS5_MFC_SYS_PWR_REG					S5P_PMUREG(0x1408)
#define EXYNOS5_G3D_SYS_PWR_REG					S5P_PMUREG(0x140C)
#define EXYNOS5_DISP1_SYS_PWR_REG				S5P_PMUREG(0x1414)
#define EXYNOS5_MAU_SYS_PWR_REG					S5P_PMUREG(0x1418)
#define EXYNOS5_CMU_CLKSTOP_GSCL_SYS_PWR_REG			S5P_PMUREG(0x1480)
#define EXYNOS5_CMU_CLKSTOP_ISP_SYS_PWR_REG			S5P_PMUREG(0x1484)
#define EXYNOS5_CMU_CLKSTOP_MFC_SYS_PWR_REG			S5P_PMUREG(0x1488)
#define EXYNOS5_CMU_CLKSTOP_G3D_SYS_PWR_REG			S5P_PMUREG(0x148C)
#define EXYNOS5_CMU_CLKSTOP_DISP1_SYS_PWR_REG			S5P_PMUREG(0x1494)
#define EXYNOS5_CMU_CLKSTOP_MAU_SYS_PWR_REG			S5P_PMUREG(0x1498)
#define EXYNOS5_CMU_SYSCLK_GSCL_SYS_PWR_REG			S5P_PMUREG(0x14C0)
#define EXYNOS5_CMU_SYSCLK_ISP_SYS_PWR_REG			S5P_PMUREG(0x14C4)
#define EXYNOS5_CMU_SYSCLK_MFC_SYS_PWR_REG			S5P_PMUREG(0x14C8)
#define EXYNOS5_CMU_SYSCLK_G3D_SYS_PWR_REG			S5P_PMUREG(0x14CC)
#define EXYNOS5_CMU_SYSCLK_DISP1_SYS_PWR_REG			S5P_PMUREG(0x14D4)
#define EXYNOS5_CMU_SYSCLK_MAU_SYS_PWR_REG			S5P_PMUREG(0x14D8)
#define EXYNOS5_CMU_RESET_GSCL_SYS_PWR_REG			S5P_PMUREG(0x1580)
#define EXYNOS5_CMU_RESET_ISP_SYS_PWR_REG			S5P_PMUREG(0x1584)
#define EXYNOS5_CMU_RESET_MFC_SYS_PWR_REG			S5P_PMUREG(0x1588)
#define EXYNOS5_CMU_RESET_G3D_SYS_PWR_REG			S5P_PMUREG(0x158C)
#define EXYNOS5_CMU_RESET_DISP1_SYS_PWR_REG			S5P_PMUREG(0x1594)
#define EXYNOS5_CMU_RESET_MAU_SYS_PWR_REG			S5P_PMUREG(0x1598)

#define EXYNOS5_ARM_CORE0_OPTION				S5P_PMUREG(0x2008)
#define EXYNOS5_ARM_CORE1_OPTION				S5P_PMUREG(0x2088)
#define EXYNOS5_FSYS_ARM_OPTION					S5P_PMUREG(0x2208)
#define EXYNOS5_ISP_ARM_OPTION					S5P_PMUREG(0x2288)
#define EXYNOS5_ARM_COMMON_OPTION				S5P_PMUREG(0x2408)
#define EXYNOS5_ARM_L2_OPTION					S5P_PMUREG(0x2608)
#define EXYNOS5_TOP_PWR_OPTION					S5P_PMUREG(0x2C48)
#define EXYNOS5_TOP_PWR_SYSMEM_OPTION				S5P_PMUREG(0x2CC8)
#define EXYNOS5_JPEG_MEM_OPTION					S5P_PMUREG(0x2F48)
#define EXYNOS5_GSCL_OPTION					S5P_PMUREG(0x4008)
#define EXYNOS5_ISP_OPTION					S5P_PMUREG(0x4028)
#define EXYNOS5_MFC_OPTION					S5P_PMUREG(0x4048)
#define EXYNOS5_G3D_OPTION					S5P_PMUREG(0x4068)
#define EXYNOS5_DISP1_OPTION					S5P_PMUREG(0x40A8)
#define EXYNOS5_MAU_OPTION					S5P_PMUREG(0x40C8)

#define EXYNOS5_USE_SC_FEEDBACK					(1 << 1)
#define EXYNOS5_USE_SC_COUNTER					(1 << 0)

#define EXYNOS5_SKIP_DEACTIVATE_ACEACP_IN_PWDN			(1 << 7)

#define EXYNOS5_OPTION_USE_STANDBYWFE				(1 << 24)
#define EXYNOS5_OPTION_USE_STANDBYWFI				(1 << 16)

#define EXYNOS5_OPTION_USE_RETENTION				(1 << 4)

#define EXYNOS5420_SWRESET_KFC_SEL				0x3

#endif /* __ASM_ARCH_REGS_PMU_H */
