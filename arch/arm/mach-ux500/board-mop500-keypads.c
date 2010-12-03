/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Keypad layouts for various boards
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/stmpe.h>
#include <linux/input/matrix_keypad.h>

#include <plat/pincfg.h>
#include <plat/ske.h>

#include <mach/devices.h>
#include <mach/hardware.h>

#include "devices-db8500.h"
#include "board-mop500.h"

/* STMPE/SKE keypad use this key layout */
static const unsigned int mop500_keymap[] = {
	KEY(2, 5, KEY_END),
	KEY(4, 1, KEY_POWER),
	KEY(3, 5, KEY_VOLUMEDOWN),
	KEY(1, 3, KEY_3),
	KEY(5, 2, KEY_RIGHT),
	KEY(5, 0, KEY_9),

	KEY(0, 5, KEY_MENU),
	KEY(7, 6, KEY_ENTER),
	KEY(4, 5, KEY_0),
	KEY(6, 7, KEY_2),
	KEY(3, 4, KEY_UP),
	KEY(3, 3, KEY_DOWN),

	KEY(6, 4, KEY_SEND),
	KEY(6, 2, KEY_BACK),
	KEY(4, 2, KEY_VOLUMEUP),
	KEY(5, 5, KEY_1),
	KEY(4, 3, KEY_LEFT),
	KEY(3, 2, KEY_7),
};

static const struct matrix_keymap_data mop500_keymap_data = {
	.keymap		= mop500_keymap,
	.keymap_size    = ARRAY_SIZE(mop500_keymap),
};

/*
 * Nomadik SKE keypad
 */
#define ROW_PIN_I0      164
#define ROW_PIN_I1      163
#define ROW_PIN_I2      162
#define ROW_PIN_I3      161
#define ROW_PIN_I4      156
#define ROW_PIN_I5      155
#define ROW_PIN_I6      154
#define ROW_PIN_I7      153
#define COL_PIN_O0      168
#define COL_PIN_O1      167
#define COL_PIN_O2      166
#define COL_PIN_O3      165
#define COL_PIN_O4      160
#define COL_PIN_O5      159
#define COL_PIN_O6      158
#define COL_PIN_O7      157

#define SKE_KPD_MAX_ROWS	8
#define SKE_KPD_MAX_COLS	8

static int ske_kp_rows[] = {
	ROW_PIN_I0, ROW_PIN_I1, ROW_PIN_I2, ROW_PIN_I3,
	ROW_PIN_I4, ROW_PIN_I5, ROW_PIN_I6, ROW_PIN_I7,
};

/*
 * ske_set_gpio_row: request and set gpio rows
 */
static int ske_set_gpio_row(int gpio)
{
	int ret;

	ret = gpio_request(gpio, "ske-kp");
	if (ret < 0) {
		pr_err("ske_set_gpio_row: gpio request failed\n");
		return ret;
	}

	ret = gpio_direction_output(gpio, 1);
	if (ret < 0) {
		pr_err("ske_set_gpio_row: gpio direction failed\n");
		gpio_free(gpio);
	}

	return ret;
}

/*
 * ske_kp_init - enable the gpio configuration
 */
static int ske_kp_init(void)
{
	int ret, i;

	for (i = 0; i < SKE_KPD_MAX_ROWS; i++) {
		ret = ske_set_gpio_row(ske_kp_rows[i]);
		if (ret < 0) {
			pr_err("ske_kp_init: failed init\n");
			return ret;
		}
	}

	return 0;
}

static struct ske_keypad_platform_data ske_keypad_board = {
	.init		= ske_kp_init,
	.keymap_data    = &mop500_keymap_data,
	.no_autorepeat  = true,
	.krow		= SKE_KPD_MAX_ROWS,     /* 8x8 matrix */
	.kcol		= SKE_KPD_MAX_COLS,
	.debounce_ms    = 40,			/* in millisecs */
};

void mop500_keypad_init(void)
{
	db8500_add_ske_keypad(&ske_keypad_board);
}
