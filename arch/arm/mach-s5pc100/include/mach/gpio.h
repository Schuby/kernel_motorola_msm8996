/* arch/arm/mach-s5pc100/include/mach/gpio.h
 *
 * Copyright 2009 Samsung Electronics Co.
 *	Byungho Min <bhmin@samsung.com>
 *
 * S5PC100 - GPIO lib support
 *
 * Base on mach-s3c6400/include/mach/gpio.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H __FILE__

/* GPIO bank sizes */
#define S5PC100_GPIO_A0_NR	(8)
#define S5PC100_GPIO_A1_NR	(5)
#define S5PC100_GPIO_B_NR	(8)
#define S5PC100_GPIO_C_NR	(5)
#define S5PC100_GPIO_D_NR	(7)
#define S5PC100_GPIO_E0_NR	(8)
#define S5PC100_GPIO_E1_NR	(6)
#define S5PC100_GPIO_F0_NR	(8)
#define S5PC100_GPIO_F1_NR	(8)
#define S5PC100_GPIO_F2_NR	(8)
#define S5PC100_GPIO_F3_NR	(4)
#define S5PC100_GPIO_G0_NR	(8)
#define S5PC100_GPIO_G1_NR	(3)
#define S5PC100_GPIO_G2_NR	(7)
#define S5PC100_GPIO_G3_NR	(7)
#define S5PC100_GPIO_H0_NR	(8)
#define S5PC100_GPIO_H1_NR	(8)
#define S5PC100_GPIO_H2_NR	(8)
#define S5PC100_GPIO_H3_NR	(8)
#define S5PC100_GPIO_I_NR	(8)
#define S5PC100_GPIO_J0_NR	(8)
#define S5PC100_GPIO_J1_NR	(5)
#define S5PC100_GPIO_J2_NR	(8)
#define S5PC100_GPIO_J3_NR	(8)
#define S5PC100_GPIO_J4_NR	(4)
#define S5PC100_GPIO_K0_NR	(8)
#define S5PC100_GPIO_K1_NR	(6)
#define S5PC100_GPIO_K2_NR	(8)
#define S5PC100_GPIO_K3_NR	(8)
#define S5PC100_GPIO_L0_NR	(8)
#define S5PC100_GPIO_L1_NR	(8)
#define S5PC100_GPIO_L2_NR	(8)
#define S5PC100_GPIO_L3_NR	(8)
#define S5PC100_GPIO_L4_NR	(8)

/* GPIO bank numbes */

/* CONFIG_S3C_GPIO_SPACE allows the user to select extra
 * space for debugging purposes so that any accidental
 * change from one gpio bank to another can be caught.
*/

#define S5PC100_GPIO_NEXT(__gpio) \
	((__gpio##_START) + (__gpio##_NR) + CONFIG_S3C_GPIO_SPACE + 1)

enum s5p_gpio_number {
	S5PC100_GPIO_A0_START	= 0,
	S5PC100_GPIO_A1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_A0),
	S5PC100_GPIO_B_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_A1),
	S5PC100_GPIO_C_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_B),
	S5PC100_GPIO_D_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_C),
	S5PC100_GPIO_E0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_D),
	S5PC100_GPIO_E1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_E0),
	S5PC100_GPIO_F0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_E1),
	S5PC100_GPIO_F1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_F0),
	S5PC100_GPIO_F2_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_F1),
	S5PC100_GPIO_F3_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_F2),
	S5PC100_GPIO_G0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_F3),
	S5PC100_GPIO_G1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_G0),
	S5PC100_GPIO_G2_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_G1),
	S5PC100_GPIO_G3_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_G2),
	S5PC100_GPIO_H0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_G3),
	S5PC100_GPIO_H1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_H0),
	S5PC100_GPIO_H2_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_H1),
	S5PC100_GPIO_H3_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_H2),
	S5PC100_GPIO_I_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_H3),
	S5PC100_GPIO_J0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_I),
	S5PC100_GPIO_J1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_J0),
	S5PC100_GPIO_J2_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_J1),
	S5PC100_GPIO_J3_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_J2),
	S5PC100_GPIO_J4_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_J3),
	S5PC100_GPIO_K0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_J4),
	S5PC100_GPIO_K1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_K0),
	S5PC100_GPIO_K2_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_K1),
	S5PC100_GPIO_K3_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_K2),
	S5PC100_GPIO_L0_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_K3),
	S5PC100_GPIO_L1_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_L0),
	S5PC100_GPIO_L2_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_L1),
	S5PC100_GPIO_L3_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_L2),
	S5PC100_GPIO_L4_START	= S5PC100_GPIO_NEXT(S5PC100_GPIO_L3),
	S5PC100_GPIO_END	= S5PC100_GPIO_NEXT(S5PC100_GPIO_L4),
};

/* S5PC100 GPIO number definitions. */
#define S5PC100_GPA0(_nr)	(S5PC100_GPIO_A0_START + (_nr))
#define S5PC100_GPA1(_nr)	(S5PC100_GPIO_A1_START + (_nr))
#define S5PC100_GPB(_nr)	(S5PC100_GPIO_B_START + (_nr))
#define S5PC100_GPC(_nr)	(S5PC100_GPIO_C_START + (_nr))
#define S5PC100_GPD(_nr)	(S5PC100_GPIO_D_START + (_nr))
#define S5PC100_GPE0(_nr)	(S5PC100_GPIO_E0_START + (_nr))
#define S5PC100_GPE1(_nr)	(S5PC100_GPIO_E1_START + (_nr))
#define S5PC100_GPF0(_nr)	(S5PC100_GPIO_F0_START + (_nr))
#define S5PC100_GPF1(_nr)	(S5PC100_GPIO_F1_START + (_nr))
#define S5PC100_GPF2(_nr)	(S5PC100_GPIO_F2_START + (_nr))
#define S5PC100_GPF3(_nr)	(S5PC100_GPIO_F3_START + (_nr))
#define S5PC100_GPG0(_nr)	(S5PC100_GPIO_G0_START + (_nr))
#define S5PC100_GPG1(_nr)	(S5PC100_GPIO_G1_START + (_nr))
#define S5PC100_GPG2(_nr)	(S5PC100_GPIO_G2_START + (_nr))
#define S5PC100_GPG3(_nr)	(S5PC100_GPIO_G3_START + (_nr))
#define S5PC100_GPH0(_nr)	(S5PC100_GPIO_H0_START + (_nr))
#define S5PC100_GPH1(_nr)	(S5PC100_GPIO_H1_START + (_nr))
#define S5PC100_GPH2(_nr)	(S5PC100_GPIO_H2_START + (_nr))
#define S5PC100_GPH3(_nr)	(S5PC100_GPIO_H3_START + (_nr))
#define S5PC100_GPI(_nr)	(S5PC100_GPIO_I_START + (_nr))
#define S5PC100_GPJ0(_nr)	(S5PC100_GPIO_J0_START + (_nr))
#define S5PC100_GPJ1(_nr)	(S5PC100_GPIO_J1_START + (_nr))
#define S5PC100_GPJ2(_nr)	(S5PC100_GPIO_J2_START + (_nr))
#define S5PC100_GPJ3(_nr)	(S5PC100_GPIO_J3_START + (_nr))
#define S5PC100_GPJ4(_nr)	(S5PC100_GPIO_J4_START + (_nr))
#define S5PC100_GPK0(_nr)	(S5PC100_GPIO_K0_START + (_nr))
#define S5PC100_GPK1(_nr)	(S5PC100_GPIO_K1_START + (_nr))
#define S5PC100_GPK2(_nr)	(S5PC100_GPIO_K2_START + (_nr))
#define S5PC100_GPK3(_nr)	(S5PC100_GPIO_K3_START + (_nr))
#define S5PC100_GPL0(_nr)	(S5PC100_GPIO_L0_START + (_nr))
#define S5PC100_GPL1(_nr)	(S5PC100_GPIO_L1_START + (_nr))
#define S5PC100_GPL2(_nr)	(S5PC100_GPIO_L2_START + (_nr))
#define S5PC100_GPL3(_nr)	(S5PC100_GPIO_L3_START + (_nr))
#define S5PC100_GPL4(_nr)	(S5PC100_GPIO_L4_START + (_nr))

/* It used the end of the S5PC100 gpios */
#define S3C_GPIO_END		S5PC100_GPIO_END

/* define the number of gpios we need to the one after the MP04() range */
#define ARCH_NR_GPIOS		(S5PC100_GPIO_END + 1)

#endif /* __ASM_ARCH_GPIO_H */
