/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#if !defined _WLAN_NV_STREAM_H
#define _WLAN_NV_STREAM_H

#include "wlan_nv_types.h"

typedef tANI_U8 _NV_STREAM_BUF;

typedef struct {
   _NV_STREAM_BUF *dataBuf;
   tANI_U32 currentIndex;
   tANI_U32 totalLength;
}_STREAM_BUF;

extern _STREAM_BUF streamBuf;

typedef enum {
   RC_FAIL,
   RC_SUCCESS,
} _STREAM_RC;

typedef enum {
   STREAM_READ,
   STREAM_WRITE,
} _STREAM_OPERATION;

_STREAM_RC nextStream (tANI_U32 *length, tANI_U8 *dataBuf);
_STREAM_RC initReadStream ( tANI_U8 *readBuf, tANI_U32 length);

#endif
