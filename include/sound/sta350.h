/*
 * Platform data for ST STA350 ASoC codec driver.
 *
 * Copyright: 2014 Raumfeld GmbH
 * Author: Sven Brandau <info@brandau.biz>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __LINUX_SND__STA350_H
#define __LINUX_SND__STA350_H

#define STA350_OCFG_2CH		0
#define STA350_OCFG_2_1CH	1
#define STA350_OCFG_1CH		3

#define STA350_OM_CH1		0
#define STA350_OM_CH2		1
#define STA350_OM_CH3		2

#define STA350_THERMAL_ADJUSTMENT_ENABLE	1
#define STA350_THERMAL_RECOVERY_ENABLE		2
#define STA350_FAULT_DETECT_RECOVERY_BYPASS	1

#define STA350_FFX_PM_DROP_COMP			0
#define STA350_FFX_PM_TAPERED_COMP		1
#define STA350_FFX_PM_FULL_POWER		2
#define STA350_FFX_PM_VARIABLE_DROP_COMP	3


struct sta350_platform_data {
	u8 output_conf;
	u8 ch1_output_mapping;
	u8 ch2_output_mapping;
	u8 ch3_output_mapping;
	u8 ffx_power_output_mode;
	u8 drop_compensation_ns;
	unsigned int thermal_warning_recovery:1;
	unsigned int thermal_warning_adjustment:1;
	unsigned int fault_detect_recovery:1;
	unsigned int oc_warning_adjustment:1;
	unsigned int max_power_use_mpcc:1;
	unsigned int max_power_correction:1;
	unsigned int am_reduction_mode:1;
	unsigned int odd_pwm_speed_mode:1;
	unsigned int distortion_compensation:1;
	unsigned int invalid_input_detect_mute:1;
};

#endif /* __LINUX_SND__STA350_H */
