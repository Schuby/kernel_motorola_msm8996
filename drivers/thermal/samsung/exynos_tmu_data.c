/*
 * exynos_tmu_data.c - Samsung EXYNOS tmu data file
 *
 *  Copyright (C) 2013 Samsung Electronics
 *  Amit Daniel Kachhap <amit.daniel@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "exynos_thermal_common.h"
#include "exynos_tmu.h"
#include "exynos_tmu_data.h"

#if defined(CONFIG_CPU_EXYNOS4210)
static const struct exynos_tmu_registers exynos4210_tmu_registers = {
	.triminfo_data = EXYNOS_TMU_REG_TRIMINFO,
	.triminfo_25_shift = EXYNOS_TRIMINFO_25_SHIFT,
	.triminfo_85_shift = EXYNOS_TRIMINFO_85_SHIFT,
	.tmu_ctrl = EXYNOS_TMU_REG_CONTROL,
	.buf_vref_sel_shift = EXYNOS_TMU_REF_VOLTAGE_SHIFT,
	.buf_vref_sel_mask = EXYNOS_TMU_REF_VOLTAGE_MASK,
	.buf_slope_sel_shift = EXYNOS_TMU_BUF_SLOPE_SEL_SHIFT,
	.buf_slope_sel_mask = EXYNOS_TMU_BUF_SLOPE_SEL_MASK,
	.core_en_shift = EXYNOS_TMU_CORE_EN_SHIFT,
	.tmu_status = EXYNOS_TMU_REG_STATUS,
	.tmu_cur_temp = EXYNOS_TMU_REG_CURRENT_TEMP,
	.threshold_temp = EXYNOS4210_TMU_REG_THRESHOLD_TEMP,
	.threshold_th0 = EXYNOS4210_TMU_REG_TRIG_LEVEL0,
	.tmu_inten = EXYNOS_TMU_REG_INTEN,
	.inten_rise_mask = EXYNOS4210_TMU_TRIG_LEVEL_MASK,
	.inten_rise0_shift = EXYNOS_TMU_INTEN_RISE0_SHIFT,
	.inten_rise1_shift = EXYNOS_TMU_INTEN_RISE1_SHIFT,
	.inten_rise2_shift = EXYNOS_TMU_INTEN_RISE2_SHIFT,
	.inten_rise3_shift = EXYNOS_TMU_INTEN_RISE3_SHIFT,
	.tmu_intstat = EXYNOS_TMU_REG_INTSTAT,
	.tmu_intclear = EXYNOS_TMU_REG_INTCLEAR,
};

struct exynos_tmu_init_data const exynos4210_default_tmu_data = {
	.tmu_data = {
		{
		.threshold = 80,
		.trigger_levels[0] = 5,
		.trigger_levels[1] = 20,
		.trigger_levels[2] = 30,
		.trigger_enable[0] = true,
		.trigger_enable[1] = true,
		.trigger_enable[2] = true,
		.trigger_enable[3] = false,
		.trigger_type[0] = THROTTLE_ACTIVE,
		.trigger_type[1] = THROTTLE_ACTIVE,
		.trigger_type[2] = SW_TRIP,
		.max_trigger_level = 4,
		.gain = 15,
		.reference_voltage = 7,
		.cal_type = TYPE_ONE_POINT_TRIMMING,
		.min_efuse_value = 40,
		.max_efuse_value = 100,
		.first_point_trim = 25,
		.second_point_trim = 85,
		.default_temp_offset = 50,
		.freq_tab[0] = {
			.freq_clip_max = 800 * 1000,
			.temp_level = 85,
			},
		.freq_tab[1] = {
			.freq_clip_max = 200 * 1000,
			.temp_level = 100,
		},
		.freq_tab_count = 2,
		.type = SOC_ARCH_EXYNOS4210,
		.registers = &exynos4210_tmu_registers,
		},
	},
	.tmu_count = 1,
};
#endif

#if defined(CONFIG_SOC_EXYNOS5250) || defined(CONFIG_SOC_EXYNOS4412)
static const struct exynos_tmu_registers exynos5250_tmu_registers = {
	.triminfo_data = EXYNOS_TMU_REG_TRIMINFO,
	.triminfo_25_shift = EXYNOS_TRIMINFO_25_SHIFT,
	.triminfo_85_shift = EXYNOS_TRIMINFO_85_SHIFT,
	.triminfo_ctrl = EXYNOS_TMU_TRIMINFO_CON,
	.triminfo_reload_shift = EXYNOS_TRIMINFO_RELOAD_SHIFT,
	.tmu_ctrl = EXYNOS_TMU_REG_CONTROL,
	.buf_vref_sel_shift = EXYNOS_TMU_REF_VOLTAGE_SHIFT,
	.buf_vref_sel_mask = EXYNOS_TMU_REF_VOLTAGE_MASK,
	.therm_trip_mode_shift = EXYNOS_TMU_TRIP_MODE_SHIFT,
	.therm_trip_mode_mask = EXYNOS_TMU_TRIP_MODE_MASK,
	.therm_trip_en_shift = EXYNOS_TMU_THERM_TRIP_EN_SHIFT,
	.buf_slope_sel_shift = EXYNOS_TMU_BUF_SLOPE_SEL_SHIFT,
	.buf_slope_sel_mask = EXYNOS_TMU_BUF_SLOPE_SEL_MASK,
	.core_en_shift = EXYNOS_TMU_CORE_EN_SHIFT,
	.tmu_status = EXYNOS_TMU_REG_STATUS,
	.tmu_cur_temp = EXYNOS_TMU_REG_CURRENT_TEMP,
	.threshold_th0 = EXYNOS_THD_TEMP_RISE,
	.threshold_th1 = EXYNOS_THD_TEMP_FALL,
	.tmu_inten = EXYNOS_TMU_REG_INTEN,
	.inten_rise_mask = EXYNOS_TMU_RISE_INT_MASK,
	.inten_rise_shift = EXYNOS_TMU_RISE_INT_SHIFT,
	.inten_fall_mask = EXYNOS_TMU_FALL_INT_MASK,
	.inten_fall_shift = EXYNOS_TMU_FALL_INT_SHIFT,
	.inten_rise0_shift = EXYNOS_TMU_INTEN_RISE0_SHIFT,
	.inten_rise1_shift = EXYNOS_TMU_INTEN_RISE1_SHIFT,
	.inten_rise2_shift = EXYNOS_TMU_INTEN_RISE2_SHIFT,
	.inten_rise3_shift = EXYNOS_TMU_INTEN_RISE3_SHIFT,
	.inten_fall0_shift = EXYNOS_TMU_INTEN_FALL0_SHIFT,
	.tmu_intstat = EXYNOS_TMU_REG_INTSTAT,
	.tmu_intclear = EXYNOS_TMU_REG_INTCLEAR,
	.emul_con = EXYNOS_EMUL_CON,
	.emul_temp_shift = EXYNOS_EMUL_DATA_SHIFT,
	.emul_time_shift = EXYNOS_EMUL_TIME_SHIFT,
	.emul_time_mask = EXYNOS_EMUL_TIME_MASK,
};

#define EXYNOS5250_TMU_DATA \
	.threshold_falling = 10, \
	.trigger_levels[0] = 85, \
	.trigger_levels[1] = 103, \
	.trigger_levels[2] = 110, \
	.trigger_levels[3] = 120, \
	.trigger_enable[0] = true, \
	.trigger_enable[1] = true, \
	.trigger_enable[2] = true, \
	.trigger_enable[3] = false, \
	.trigger_type[0] = THROTTLE_ACTIVE, \
	.trigger_type[1] = THROTTLE_ACTIVE, \
	.trigger_type[2] = SW_TRIP, \
	.trigger_type[3] = HW_TRIP, \
	.max_trigger_level = 4, \
	.gain = 8, \
	.reference_voltage = 16, \
	.noise_cancel_mode = 4, \
	.cal_type = TYPE_ONE_POINT_TRIMMING, \
	.efuse_value = 55, \
	.min_efuse_value = 40, \
	.max_efuse_value = 100, \
	.first_point_trim = 25, \
	.second_point_trim = 85, \
	.default_temp_offset = 50, \
	.freq_tab[0] = { \
		.freq_clip_max = 800 * 1000, \
		.temp_level = 85, \
	}, \
	.freq_tab[1] = { \
		.freq_clip_max = 200 * 1000, \
		.temp_level = 103, \
	}, \
	.freq_tab_count = 2, \
	.type = SOC_ARCH_EXYNOS, \
	.registers = &exynos5250_tmu_registers,

struct exynos_tmu_init_data const exynos5250_default_tmu_data = {
	.tmu_data = {
		{ EXYNOS5250_TMU_DATA },
	},
	.tmu_count = 1,
};
#endif
