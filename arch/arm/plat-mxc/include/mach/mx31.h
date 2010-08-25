#ifndef __MACH_MX31_H__
#define __MACH_MX31_H__

#ifndef __ASSEMBLER__
#include <linux/io.h>
#endif

/*
 * IRAM
 */
#define MX31_IRAM_BASE_ADDR		0x1ffc0000	/* internal ram */
#define MX31_IRAM_SIZE			SZ_16K

#define MX31_L2CC_BASE_ADDR		0x30000000
#define MX31_L2CC_SIZE			SZ_1M

#define MX31_AIPS1_BASE_ADDR		0x43f00000
#define MX31_AIPS1_BASE_ADDR_VIRT	0xfc000000
#define MX31_AIPS1_SIZE			SZ_1M
#define MX31_MAX_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x04000)
#define MX31_EVTMON_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x08000)
#define MX31_CLKCTL_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x0c000)
#define MX31_ETB_SLOT4_BASE_ADDR		(MX31_AIPS1_BASE_ADDR + 0x10000)
#define MX31_ETB_SLOT5_BASE_ADDR		(MX31_AIPS1_BASE_ADDR + 0x14000)
#define MX31_ECT_CTIO_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x18000)
#define MX31_I2C1_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x80000)
#define MX31_I2C3_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x84000)
#define MX31_OTG_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x88000)
#define MX31_ATA_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x8c000)
#define MX31_UART1_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x90000)
#define MX31_UART2_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x94000)
#define MX31_I2C2_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x98000)
#define MX31_OWIRE_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0x9c000)
#define MX31_SSI1_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xa0000)
#define MX31_CSPI1_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xa4000)
#define MX31_KPP_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xa8000)
#define MX31_IOMUXC_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xac000)
#define MX31_UART4_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xb0000)
#define MX31_UART5_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xb4000)
#define MX31_ECT_IP1_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xb8000)
#define MX31_ECT_IP2_BASE_ADDR			(MX31_AIPS1_BASE_ADDR + 0xbc000)

#define MX31_SPBA0_BASE_ADDR		0x50000000
#define MX31_SPBA0_BASE_ADDR_VIRT	0xfc100000
#define MX31_SPBA0_SIZE			SZ_1M
#define MX31_MMC_SDHC1_BASE_ADDR		(MX31_SPBA0_BASE_ADDR + 0x04000)
#define MX31_MMC_SDHC2_BASE_ADDR		(MX31_SPBA0_BASE_ADDR + 0x08000)
#define MX31_UART3_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x0c000)
#define MX31_CSPI2_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x10000)
#define MX31_SSI2_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x14000)
#define MX31_SIM1_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x18000)
#define MX31_IIM_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x1c000)
#define MX31_ATA_DMA_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x20000)
#define MX31_MSHC1_BASE_ADDR			(MX31_SPBA0_BASE_ADDR + 0x24000)
#define MX31_SPBA_CTRL_BASE_ADDR		(MX31_SPBA0_BASE_ADDR + 0x3c000)

#define MX31_AIPS2_BASE_ADDR		0x53f00000
#define MX31_AIPS2_BASE_ADDR_VIRT	0xfc200000
#define MX31_AIPS2_SIZE			SZ_1M
#define MX31_CCM_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0x80000)
#define MX31_CSPI3_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0x84000)
#define MX31_FIRI_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0x8c000)
#define MX31_GPT1_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0x90000)
#define MX31_EPIT1_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0x94000)
#define MX31_EPIT2_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0x98000)
#define MX31_GPIO3_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xa4000)
#define MX31_SCC_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xac000)
#define MX31_SCM_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xae000)
#define MX31_SMN_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xaf000)
#define MX31_RNGA_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xb0000)
#define MX31_IPU_CTRL_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xc0000)
#define MX31_AUDMUX_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xc4000)
#define MX31_MPEG4_ENC_BASE_ADDR		(MX31_AIPS2_BASE_ADDR + 0xc8000)
#define MX31_GPIO1_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xcc000)
#define MX31_GPIO2_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xd0000)
#define MX31_SDMA_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xd4000)
#define MX31_RTC_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xd8000)
#define MX31_WDOG_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xdc000)
#define MX31_PWM_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xe0000)
#define MX31_RTIC_BASE_ADDR			(MX31_AIPS2_BASE_ADDR + 0xec000)

#define MX31_ROMP_BASE_ADDR		0x60000000
#define MX31_ROMP_BASE_ADDR_VIRT	0xfc500000
#define MX31_ROMP_SIZE			SZ_1M

#define MX31_AVIC_BASE_ADDR		0x68000000
#define MX31_AVIC_BASE_ADDR_VIRT	0xfc400000
#define MX31_AVIC_SIZE			SZ_1M

#define MX31_IPU_MEM_BASE_ADDR		0x70000000
#define MX31_CSD0_BASE_ADDR		0x80000000
#define MX31_CSD1_BASE_ADDR		0x90000000

#define MX31_CS0_BASE_ADDR		0xa0000000
#define MX31_CS1_BASE_ADDR		0xa8000000
#define MX31_CS2_BASE_ADDR		0xb0000000
#define MX31_CS3_BASE_ADDR		0xb2000000

#define MX31_CS4_BASE_ADDR		0xb4000000
#define MX31_CS4_BASE_ADDR_VIRT		0xf4000000
#define MX31_CS4_SIZE			SZ_32M

#define MX31_CS5_BASE_ADDR		0xb6000000
#define MX31_CS5_BASE_ADDR_VIRT		0xf6000000
#define MX31_CS5_SIZE			SZ_32M

#define MX31_X_MEMC_BASE_ADDR		0xb8000000
#define MX31_X_MEMC_BASE_ADDR_VIRT	0xfc320000
#define MX31_X_MEMC_SIZE		SZ_64K
#define MX31_NFC_BASE_ADDR			(MX31_X_MEMC_BASE_ADDR + 0x0000)
#define MX31_ESDCTL_BASE_ADDR			(MX31_X_MEMC_BASE_ADDR + 0x1000)
#define MX31_WEIM_BASE_ADDR			(MX31_X_MEMC_BASE_ADDR + 0x2000)
#define MX31_M3IF_BASE_ADDR			(MX31_X_MEMC_BASE_ADDR + 0x3000)
#define MX31_EMI_CTL_BASE_ADDR			(MX31_X_MEMC_BASE_ADDR + 0x4000)
#define MX31_PCMCIA_CTL_BASE_ADDR		MX31_EMI_CTL_BASE_ADDR

#define MX31_WEIM_CSCRx_BASE_ADDR(cs)	(MX31_WEIM_BASE_ADDR + (cs) * 0x10)
#define MX31_WEIM_CSCRxU(cs)			(MX31_WEIM_CSCRx_BASE_ADDR(cs))
#define MX31_WEIM_CSCRxL(cs)			(MX31_WEIM_CSCRx_BASE_ADDR(cs) + 0x4)
#define MX31_WEIM_CSCRxA(cs)			(MX31_WEIM_CSCRx_BASE_ADDR(cs) + 0x8)

#define MX31_PCMCIA_MEM_BASE_ADDR	0xbc000000

#define MX31_IO_ADDRESS(x) (						\
	IMX_IO_ADDRESS(x, MX31_AIPS1) ?:				\
	IMX_IO_ADDRESS(x, MX31_AIPS2) ?:				\
	IMX_IO_ADDRESS(x, MX31_AVIC) ?:					\
	IMX_IO_ADDRESS(x, MX31_X_MEMC) ?:				\
	IMX_IO_ADDRESS(x, MX31_SPBA0))

#ifndef __ASSEMBLER__
static inline void mx31_setup_weimcs(size_t cs,
		unsigned upper, unsigned lower, unsigned addional)
{
	__raw_writel(upper, MX31_IO_ADDRESS(MX31_WEIM_CSCRxU(cs)));
	__raw_writel(lower, MX31_IO_ADDRESS(MX31_WEIM_CSCRxL(cs)));
	__raw_writel(addional, MX31_IO_ADDRESS(MX31_WEIM_CSCRxA(cs)));
}
#endif

#define MX31_INT_I2C3		3
#define MX31_INT_I2C2		4
#define MX31_INT_MPEG4_ENCODER	5
#define MX31_INT_RTIC		6
#define MX31_INT_FIRI		7
#define MX31_INT_MMC_SDHC2	8
#define MX31_INT_MMC_SDHC1	9
#define MX31_INT_I2C1		10
#define MX31_INT_SSI2		11
#define MX31_INT_SSI1		12
#define MX31_INT_CSPI2		13
#define MX31_INT_CSPI1		14
#define MX31_INT_ATA		15
#define MX31_INT_MBX		16
#define MX31_INT_CSPI3		17
#define MX31_INT_UART3		18
#define MX31_INT_IIM		19
#define MX31_INT_SIM2		20
#define MX31_INT_SIM1		21
#define MX31_INT_RNGA		22
#define MX31_INT_EVTMON		23
#define MX31_INT_KPP		24
#define MX31_INT_RTC		25
#define MX31_INT_PWM		26
#define MX31_INT_EPIT2		27
#define MX31_INT_EPIT1		28
#define MX31_INT_GPT		29
#define MX31_INT_POWER_FAIL	30
#define MX31_INT_CCM_DVFS	31
#define MX31_INT_UART2		32
#define MX31_INT_NANDFC		33
#define MX31_INT_SDMA		34
#define MX31_INT_USB1		35
#define MX31_INT_USB2		36
#define MX31_INT_USB3		37
#define MX31_INT_USB4		38
#define MX31_INT_MSHC1		39
#define MX31_INT_MSHC2		40
#define MX31_INT_IPU_ERR	41
#define MX31_INT_IPU_SYN	42
#define MX31_INT_UART1		45
#define MX31_INT_UART4		46
#define MX31_INT_UART5		47
#define MX31_INT_ECT		48
#define MX31_INT_SCC_SCM	49
#define MX31_INT_SCC_SMN	50
#define MX31_INT_GPIO2		51
#define MX31_INT_GPIO1		52
#define MX31_INT_CCM		53
#define MX31_INT_PCMCIA		54
#define MX31_INT_WDOG		55
#define MX31_INT_GPIO3		56
#define MX31_INT_EXT_POWER	58
#define MX31_INT_EXT_TEMPER	59
#define MX31_INT_EXT_SENSOR60	60
#define MX31_INT_EXT_SENSOR61	61
#define MX31_INT_EXT_WDOG	62
#define MX31_INT_EXT_TV		63

#define MX31_DMA_REQ_SSI2_RX1	22
#define MX31_DMA_REQ_SSI2_TX1	23
#define MX31_DMA_REQ_SSI2_RX0	24
#define MX31_DMA_REQ_SSI2_TX0	25
#define MX31_DMA_REQ_SSI1_RX1	26
#define MX31_DMA_REQ_SSI1_TX1	27
#define MX31_DMA_REQ_SSI1_RX0	28
#define MX31_DMA_REQ_SSI1_TX0	29

#define MX31_PROD_SIGNATURE		0x1	/* For MX31 */

/* silicon revisions specific to i.MX31 */
#define MX31_CHIP_REV_1_0		0x10
#define MX31_CHIP_REV_1_1		0x11
#define MX31_CHIP_REV_1_2		0x12
#define MX31_CHIP_REV_1_3		0x13
#define MX31_CHIP_REV_2_0		0x20
#define MX31_CHIP_REV_2_1		0x21
#define MX31_CHIP_REV_2_2		0x22
#define MX31_CHIP_REV_2_3		0x23
#define MX31_CHIP_REV_3_0		0x30
#define MX31_CHIP_REV_3_1		0x31
#define MX31_CHIP_REV_3_2		0x32

#define MX31_SYSTEM_REV_MIN		MX31_CHIP_REV_1_0
#define MX31_SYSTEM_REV_NUM		3

#ifdef IMX_NEEDS_DEPRECATED_SYMBOLS
/* these should go away */
#define ATA_BASE_ADDR MX31_ATA_BASE_ADDR
#define UART4_BASE_ADDR MX31_UART4_BASE_ADDR
#define UART5_BASE_ADDR MX31_UART5_BASE_ADDR
#define MMC_SDHC1_BASE_ADDR MX31_MMC_SDHC1_BASE_ADDR
#define MMC_SDHC2_BASE_ADDR MX31_MMC_SDHC2_BASE_ADDR
#define SIM1_BASE_ADDR MX31_SIM1_BASE_ADDR
#define IIM_BASE_ADDR MX31_IIM_BASE_ADDR
#define CSPI3_BASE_ADDR MX31_CSPI3_BASE_ADDR
#define FIRI_BASE_ADDR MX31_FIRI_BASE_ADDR
#define SCM_BASE_ADDR MX31_SCM_BASE_ADDR
#define SMN_BASE_ADDR MX31_SMN_BASE_ADDR
#define MPEG4_ENC_BASE_ADDR MX31_MPEG4_ENC_BASE_ADDR
#define MXC_INT_MPEG4_ENCODER MX31_INT_MPEG4_ENCODER
#define MXC_INT_FIRI MX31_INT_FIRI
#define MXC_INT_MMC_SDHC1 MX31_INT_MMC_SDHC1
#define MXC_INT_MBX MX31_INT_MBX
#define MXC_INT_CSPI3 MX31_INT_CSPI3
#define MXC_INT_SIM2 MX31_INT_SIM2
#define MXC_INT_SIM1 MX31_INT_SIM1
#define MXC_INT_CCM_DVFS MX31_INT_CCM_DVFS
#define MXC_INT_USB1 MX31_INT_USB1
#define MXC_INT_USB2 MX31_INT_USB2
#define MXC_INT_USB3 MX31_INT_USB3
#define MXC_INT_USB4 MX31_INT_USB4
#define MXC_INT_MSHC2 MX31_INT_MSHC2
#define MXC_INT_UART4 MX31_INT_UART4
#define MXC_INT_UART5 MX31_INT_UART5
#define MXC_INT_CCM MX31_INT_CCM
#define MXC_INT_PCMCIA MX31_INT_PCMCIA
#endif

#endif /* ifndef __MACH_MX31_H__ */
