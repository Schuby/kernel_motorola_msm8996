/*
 *  Copyright (C) 2001 Ben. Herrenschmidt (benh@kernel.crashing.org)
 *
 *  Modifications for ppc64:
 *      Copyright (C) 2003 Dave Engebretsen <engebret@us.ibm.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_POWERPC_FIRMWARE_H
#define __ASM_POWERPC_FIRMWARE_H

#ifdef __KERNEL__

#include <asm/asm-compat.h>
#include <asm/feature-fixups.h>

/* firmware feature bitmask values */
#define FIRMWARE_MAX_FEATURES 63

#define FW_FEATURE_PFT		ASM_CONST(0x0000000000000001)
#define FW_FEATURE_TCE		ASM_CONST(0x0000000000000002)
#define FW_FEATURE_SPRG0	ASM_CONST(0x0000000000000004)
#define FW_FEATURE_DABR		ASM_CONST(0x0000000000000008)
#define FW_FEATURE_COPY		ASM_CONST(0x0000000000000010)
#define FW_FEATURE_ASR		ASM_CONST(0x0000000000000020)
#define FW_FEATURE_DEBUG	ASM_CONST(0x0000000000000040)
#define FW_FEATURE_TERM		ASM_CONST(0x0000000000000080)
#define FW_FEATURE_PERF		ASM_CONST(0x0000000000000100)
#define FW_FEATURE_DUMP		ASM_CONST(0x0000000000000200)
#define FW_FEATURE_INTERRUPT	ASM_CONST(0x0000000000000400)
#define FW_FEATURE_MIGRATE	ASM_CONST(0x0000000000000800)
#define FW_FEATURE_PERFMON	ASM_CONST(0x0000000000001000)
#define FW_FEATURE_CRQ		ASM_CONST(0x0000000000002000)
#define FW_FEATURE_VIO		ASM_CONST(0x0000000000004000)
#define FW_FEATURE_RDMA		ASM_CONST(0x0000000000008000)
#define FW_FEATURE_LLAN		ASM_CONST(0x0000000000010000)
#define FW_FEATURE_BULK_REMOVE	ASM_CONST(0x0000000000020000)
#define FW_FEATURE_XDABR	ASM_CONST(0x0000000000040000)
#define FW_FEATURE_MULTITCE	ASM_CONST(0x0000000000080000)
#define FW_FEATURE_SPLPAR	ASM_CONST(0x0000000000100000)
#define FW_FEATURE_LPAR		ASM_CONST(0x0000000000400000)
#define FW_FEATURE_PS3_LV1	ASM_CONST(0x0000000000800000)
#define FW_FEATURE_BEAT		ASM_CONST(0x0000000001000000)
#define FW_FEATURE_CMO		ASM_CONST(0x0000000002000000)
#define FW_FEATURE_VPHN		ASM_CONST(0x0000000004000000)
#define FW_FEATURE_XCMO		ASM_CONST(0x0000000008000000)
#define FW_FEATURE_OPAL		ASM_CONST(0x0000000010000000)
#define FW_FEATURE_OPALv2	ASM_CONST(0x0000000020000000)
#define FW_FEATURE_SET_MODE	ASM_CONST(0x0000000040000000)

#ifndef __ASSEMBLY__

enum {
#ifdef CONFIG_PPC64
	FW_FEATURE_PSERIES_POSSIBLE = FW_FEATURE_PFT | FW_FEATURE_TCE |
		FW_FEATURE_SPRG0 | FW_FEATURE_DABR | FW_FEATURE_COPY |
		FW_FEATURE_ASR | FW_FEATURE_DEBUG | FW_FEATURE_TERM |
		FW_FEATURE_PERF | FW_FEATURE_DUMP | FW_FEATURE_INTERRUPT |
		FW_FEATURE_MIGRATE | FW_FEATURE_PERFMON | FW_FEATURE_CRQ |
		FW_FEATURE_VIO | FW_FEATURE_RDMA | FW_FEATURE_LLAN |
		FW_FEATURE_BULK_REMOVE | FW_FEATURE_XDABR |
		FW_FEATURE_MULTITCE | FW_FEATURE_SPLPAR | FW_FEATURE_LPAR |
		FW_FEATURE_CMO | FW_FEATURE_VPHN | FW_FEATURE_XCMO |
		FW_FEATURE_SET_MODE,
	FW_FEATURE_PSERIES_ALWAYS = 0,
	FW_FEATURE_POWERNV_POSSIBLE = FW_FEATURE_OPAL | FW_FEATURE_OPALv2,
	FW_FEATURE_POWERNV_ALWAYS = 0,
	FW_FEATURE_PS3_POSSIBLE = FW_FEATURE_LPAR | FW_FEATURE_PS3_LV1,
	FW_FEATURE_PS3_ALWAYS = FW_FEATURE_LPAR | FW_FEATURE_PS3_LV1,
	FW_FEATURE_CELLEB_POSSIBLE = FW_FEATURE_LPAR | FW_FEATURE_BEAT,
	FW_FEATURE_CELLEB_ALWAYS = 0,
	FW_FEATURE_NATIVE_POSSIBLE = 0,
	FW_FEATURE_NATIVE_ALWAYS = 0,
	FW_FEATURE_POSSIBLE =
#ifdef CONFIG_PPC_PSERIES
		FW_FEATURE_PSERIES_POSSIBLE |
#endif
#ifdef CONFIG_PPC_POWERNV
		FW_FEATURE_POWERNV_POSSIBLE |
#endif
#ifdef CONFIG_PPC_PS3
		FW_FEATURE_PS3_POSSIBLE |
#endif
#ifdef CONFIG_PPC_CELLEB
		FW_FEATURE_CELLEB_POSSIBLE |
#endif
#ifdef CONFIG_PPC_NATIVE
		FW_FEATURE_NATIVE_ALWAYS |
#endif
		0,
	FW_FEATURE_ALWAYS =
#ifdef CONFIG_PPC_PSERIES
		FW_FEATURE_PSERIES_ALWAYS &
#endif
#ifdef CONFIG_PPC_POWERNV
		FW_FEATURE_POWERNV_ALWAYS &
#endif
#ifdef CONFIG_PPC_PS3
		FW_FEATURE_PS3_ALWAYS &
#endif
#ifdef CONFIG_PPC_CELLEB
		FW_FEATURE_CELLEB_ALWAYS &
#endif
#ifdef CONFIG_PPC_NATIVE
		FW_FEATURE_NATIVE_ALWAYS &
#endif
		FW_FEATURE_POSSIBLE,

#else /* CONFIG_PPC64 */
	FW_FEATURE_POSSIBLE = 0,
	FW_FEATURE_ALWAYS = 0,
#endif
};

/* This is used to identify firmware features which are available
 * to the kernel.
 */
extern unsigned long	powerpc_firmware_features;

#define firmware_has_feature(feature)					\
	((FW_FEATURE_ALWAYS & (feature)) ||				\
		(FW_FEATURE_POSSIBLE & powerpc_firmware_features & (feature)))

extern void system_reset_fwnmi(void);
extern void machine_check_fwnmi(void);

/* This is true if we are using the firmware NMI handler (typically LPAR) */
extern int fwnmi_active;

extern unsigned int __start___fw_ftr_fixup, __stop___fw_ftr_fixup;

#endif /* __ASSEMBLY__ */
#endif /* __KERNEL__ */
#endif /* __ASM_POWERPC_FIRMWARE_H */
