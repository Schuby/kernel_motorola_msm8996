/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */


#ifndef EPPING_MAIN_H
#define EPPING_MAIN_H
/**===========================================================================

  \file  epping_main.h

  \brief Linux epping head file

  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <vos_lock.h>

#define WLAN_EPPING_ENABLE_BIT          (1 << 8)
#define WLAN_EPPING_IRQ_BIT             (1 << 9)
#define WLAN_EPPING_FW_UART_BIT         (1 << 10)
#define WLAN_IS_EPPING_ENABLED(x)       (x & WLAN_EPPING_ENABLE_BIT)
#define WLAN_IS_EPPING_IRQ(x)           (x & WLAN_EPPING_IRQ_BIT)
#define WLAN_IS_EPPING_FW_UART(x)       (x & WLAN_EPPING_FW_UART_BIT)

/* epping_main signatures */
int epping_driver_init(int con_mode, vos_wake_lock_t *g_wake_lock,
                       char *pwlan_module_name);
void epping_driver_exit(v_CONTEXT_t pVosContext);
void epping_exit(v_CONTEXT_t pVosContext);
int epping_wlan_startup(struct device *dev, v_VOID_t *hif_sc);
#endif /* end #ifndef EPPING_MAIN_H */
