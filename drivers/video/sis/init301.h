/* $XFree86$ */
/* $XdotOrg$ */
/*
 * Data and prototypes for init301.c
 *
 * Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria
 *
 * If distributed as part of the Linux kernel, the following license terms
 * apply:
 *
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License as published by
 * * the Free Software Foundation; either version 2 of the named License,
 * * or any later version.
 * *
 * * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for more details.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with this program; if not, write to the Free Software
 * * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Otherwise, the following license terms apply:
 *
 * * Redistribution and use in source and binary forms, with or without
 * * modification, are permitted provided that the following conditions
 * * are met:
 * * 1) Redistributions of source code must retain the above copyright
 * *    notice, this list of conditions and the following disclaimer.
 * * 2) Redistributions in binary form must reproduce the above copyright
 * *    notice, this list of conditions and the following disclaimer in the
 * *    documentation and/or other materials provided with the distribution.
 * * 3) The name of the author may not be used to endorse or promote products
 * *    derived from this software without specific prior written permission.
 * *
 * * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: 	Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

#ifndef  _INIT301_H_
#define  _INIT301_H_

#include "osdef.h"
#include "initdef.h"

#include "vgatypes.h"
#include "vstruct.h"
#ifdef SIS_CP
#undef SIS_CP
#endif
#include <linux/types.h>
#include <asm/io.h>
#include <linux/fb.h>
#include "sis.h"
#include <video/sisfb.h>

static const unsigned char SiS_YPbPrTable[3][64] = {
  {
    0x17,0x1d,0x03,0x09,0x05,0x06,0x0c,0x0c,
    0x94,0x49,0x01,0x0a,0x06,0x0d,0x04,0x0a,
    0x06,0x14,0x0d,0x04,0x0a,0x00,0x85,0x1b,
    0x0c,0x50,0x00,0x97,0x00,0xda,0x4a,0x17,
    0x7d,0x05,0x4b,0x00,0x00,0xe2,0x00,0x02,
    0x03,0x0a,0x65,0x9d /*0x8d*/,0x08,0x92,0x8f,0x40,
    0x60,0x80,0x14,0x90,0x8c,0x60,0x14,0x53 /*0x50*/,
    0x00,0x40,0x44,0x00,0xdb,0x02,0x3b,0x00
  },
  {
    0x33,0x06,0x06,0x09,0x0b,0x0c,0x0c,0x0c,
    0x98,0x0a,0x01,0x0d,0x06,0x0d,0x04,0x0a,
    0x06,0x14,0x0d,0x04,0x0a,0x00,0x85,0x3f,
    0x0c,0x50,0xb2,0x9f,0x16,0x59,0x4f,0x13,
    0xad,0x11,0xad,0x1d,0x40,0x8a,0x3d,0xb8,
    0x51,0x5e,0x60,0x49,0x7d,0x92,0x0f,0x40,
    0x60,0x80,0x14,0x90,0x8c,0x60,0x14,0x4e,
    0x43,0x41,0x11,0x00,0xfc,0xff,0x32,0x00
  },
  {
#if 0 /* OK, but sticks to left edge */
    0x13,0x1d,0xe8,0x09,0x09,0xed,0x0c,0x0c,
    0x98,0x0a,0x01,0x0c,0x06,0x0d,0x04,0x0a,
    0x06,0x14,0x0d,0x04,0x0a,0x00,0x85,0x3f,
    0xed,0x50,0x70,0x9f,0x16,0x59,0x21 /*0x2b*/,0x13,
    0x27,0x0b,0x27,0xfc,0x30,0x27,0x1c,0xb0,
    0x4b,0x4b,0x65 /*0x6f*/,0x2f,0x63,0x92,0x0f,0x40,
    0x60,0x80,0x14,0x90,0x8c,0x60,0x14,0x27,
    0x00,0x40,0x11,0x00,0xfc,0xff,0x32,0x00
#endif
#if 1 /* Perfect */
    0x23,0x2d,0xe8,0x09,0x09,0xed,0x0c,0x0c,
    0x98,0x0a,0x01,0x0c,0x06,0x0d,0x04,0x0a,
    0x06,0x14,0x0d,0x04,0x0a,0x00,0x85,0x3f,
    0xed,0x50,0x70,0x9f,0x16,0x59,0x60,0x13,
    0x27,0x0b,0x27,0xfc,0x30,0x27,0x1c,0xb0,
    0x4b,0x4b,0x6f,0x2f,0x63,0x92,0x0f,0x40,
    0x60,0x80,0x14,0x90,0x8c,0x60,0x14,0x73,
    0x00,0x40,0x11,0x00,0xfc,0xff,0x32,0x00
#endif
  }
};

static const unsigned char SiS_TVPhase[] =
{
	0x21,0xED,0xBA,0x08,	/* 0x00 SiS_NTSCPhase */
	0x2A,0x05,0xE3,0x00,	/* 0x01 SiS_PALPhase */
	0x21,0xE4,0x2E,0x9B,	/* 0x02 SiS_PALMPhase */
	0x21,0xF4,0x3E,0xBA,	/* 0x03 SiS_PALNPhase */
	0x1E,0x8B,0xA2,0xA7,
	0x1E,0x83,0x0A,0xE0,	/* 0x05 SiS_SpecialPhaseM */
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x21,0xF0,0x7B,0xD6,	/* 0x08 SiS_NTSCPhase2 */
	0x2A,0x09,0x86,0xE9,	/* 0x09 SiS_PALPhase2 */
	0x21,0xE6,0xEF,0xA4,	/* 0x0a SiS_PALMPhase2 */
	0x21,0xF6,0x94,0x46,	/* 0x0b SiS_PALNPhase2 */
	0x1E,0x8B,0xA2,0xA7,
	0x1E,0x83,0x0A,0xE0,	/* 0x0d SiS_SpecialPhaseM */
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x1e,0x8c,0x5c,0x7a,	/* 0x10 SiS_SpecialPhase */
	0x25,0xd4,0xfd,0x5e	/* 0x11 SiS_SpecialPhaseJ */
};

static const unsigned char SiS_HiTVGroup3_1[] = {
    0x00, 0x14, 0x15, 0x25, 0x55, 0x15, 0x0b, 0x13,
    0xb1, 0x41, 0x62, 0x62, 0xff, 0xf4, 0x45, 0xa6,
    0x25, 0x2f, 0x67, 0xf6, 0xbf, 0xff, 0x8e, 0x20,
    0xac, 0xda, 0x60, 0xfe, 0x6a, 0x9a, 0x06, 0x10,
    0xd1, 0x04, 0x18, 0x0a, 0xff, 0x80, 0x00, 0x80,
    0x3b, 0x77, 0x00, 0xef, 0xe0, 0x10, 0xb0, 0xe0,
    0x10, 0x4f, 0x0f, 0x0f, 0x05, 0x0f, 0x08, 0x6e,
    0x1a, 0x1f, 0x25, 0x2a, 0x4c, 0xaa, 0x01
};

static const unsigned char SiS_HiTVGroup3_2[] = {
    0x00, 0x14, 0x15, 0x25, 0x55, 0x15, 0x0b, 0x7a,
    0x54, 0x41, 0xe7, 0xe7, 0xff, 0xf4, 0x45, 0xa6,
    0x25, 0x2f, 0x67, 0xf6, 0xbf, 0xff, 0x8e, 0x20,
    0xac, 0x6a, 0x60, 0x2b, 0x52, 0xcd, 0x61, 0x10,
    0x51, 0x04, 0x18, 0x0a, 0x1f, 0x80, 0x00, 0x80,
    0xff, 0xa4, 0x04, 0x2b, 0x94, 0x21, 0x72, 0x94,
    0x26, 0x05, 0x01, 0x0f, 0xed, 0x0f, 0x0a, 0x64,
    0x18, 0x1d, 0x23, 0x28, 0x4c, 0xaa, 0x01
};

/* 301C / 302ELV extended Part2 TV registers (4 tap scaler) */

static const unsigned char SiS_Part2CLVX_1[] = {
    0x00,0x00,
    0x00,0x20,0x00,0x00,0x7F,0x20,0x02,0x7F,0x7D,0x20,0x04,0x7F,0x7D,0x1F,0x06,0x7E,
    0x7C,0x1D,0x09,0x7E,0x7C,0x1B,0x0B,0x7E,0x7C,0x19,0x0E,0x7D,0x7C,0x17,0x11,0x7C,
    0x7C,0x14,0x14,0x7C,0x7C,0x11,0x17,0x7C,0x7D,0x0E,0x19,0x7C,0x7E,0x0B,0x1B,0x7C,
    0x7E,0x09,0x1D,0x7C,0x7F,0x06,0x1F,0x7C,0x7F,0x04,0x20,0x7D,0x00,0x02,0x20,0x7E
};

static const unsigned char SiS_Part2CLVX_2[] = {
    0x00,0x00,
    0x00,0x20,0x00,0x00,0x7F,0x20,0x02,0x7F,0x7D,0x20,0x04,0x7F,0x7D,0x1F,0x06,0x7E,
    0x7C,0x1D,0x09,0x7E,0x7C,0x1B,0x0B,0x7E,0x7C,0x19,0x0E,0x7D,0x7C,0x17,0x11,0x7C,
    0x7C,0x14,0x14,0x7C,0x7C,0x11,0x17,0x7C,0x7D,0x0E,0x19,0x7C,0x7E,0x0B,0x1B,0x7C,
    0x7E,0x09,0x1D,0x7C,0x7F,0x06,0x1F,0x7C,0x7F,0x04,0x20,0x7D,0x00,0x02,0x20,0x7E
};

static const unsigned char SiS_Part2CLVX_3[] = {  /* NTSC, 525i, 525p */
    0xE0,0x01,
    0x04,0x1A,0x04,0x7E,0x03,0x1A,0x06,0x7D,0x01,0x1A,0x08,0x7D,0x00,0x19,0x0A,0x7D,
    0x7F,0x19,0x0C,0x7C,0x7E,0x18,0x0E,0x7C,0x7E,0x17,0x10,0x7B,0x7D,0x15,0x12,0x7C,
    0x7D,0x13,0x13,0x7D,0x7C,0x12,0x15,0x7D,0x7C,0x10,0x17,0x7D,0x7C,0x0E,0x18,0x7E,
    0x7D,0x0C,0x19,0x7E,0x7D,0x0A,0x19,0x00,0x7D,0x08,0x1A,0x01,0x7E,0x06,0x1A,0x02,
    0x58,0x02,
    0x07,0x14,0x07,0x7E,0x06,0x14,0x09,0x7D,0x05,0x14,0x0A,0x7D,0x04,0x13,0x0B,0x7E,
    0x03,0x13,0x0C,0x7E,0x02,0x12,0x0D,0x7F,0x01,0x12,0x0E,0x7F,0x01,0x11,0x0F,0x7F,
    0x00,0x10,0x10,0x00,0x7F,0x0F,0x11,0x01,0x7F,0x0E,0x12,0x01,0x7E,0x0D,0x12,0x03,
    0x7E,0x0C,0x13,0x03,0x7E,0x0B,0x13,0x04,0x7E,0x0A,0x14,0x04,0x7D,0x09,0x14,0x06,
    0x00,0x03,
    0x09,0x0F,0x09,0x7F,0x08,0x0F,0x09,0x00,0x07,0x0F,0x0A,0x00,0x06,0x0F,0x0A,0x01,
    0x06,0x0E,0x0B,0x01,0x05,0x0E,0x0B,0x02,0x04,0x0E,0x0C,0x02,0x04,0x0D,0x0C,0x03,
    0x03,0x0D,0x0D,0x03,0x02,0x0C,0x0D,0x05,0x02,0x0C,0x0E,0x04,0x01,0x0B,0x0E,0x06,
    0x01,0x0B,0x0E,0x06,0x00,0x0A,0x0F,0x07,0x00,0x0A,0x0F,0x07,0x00,0x09,0x0F,0x08,
    0xFF,0xFF
};

static const unsigned char SiS_Part2CLVX_4[] = {   /* PAL */
    0x58,0x02,
    0x05,0x19,0x05,0x7D,0x03,0x19,0x06,0x7E,0x02,0x19,0x08,0x7D,0x01,0x18,0x0A,0x7D,
    0x00,0x18,0x0C,0x7C,0x7F,0x17,0x0E,0x7C,0x7E,0x16,0x0F,0x7D,0x7E,0x14,0x11,0x7D,
    0x7D,0x13,0x13,0x7D,0x7D,0x11,0x14,0x7E,0x7D,0x0F,0x16,0x7E,0x7D,0x0E,0x17,0x7E,
    0x7D,0x0C,0x18,0x7F,0x7D,0x0A,0x18,0x01,0x7D,0x08,0x19,0x02,0x7D,0x06,0x19,0x04,
    0x00,0x03,
    0x08,0x12,0x08,0x7E,0x07,0x12,0x09,0x7E,0x06,0x12,0x0A,0x7E,0x05,0x11,0x0B,0x7F,
    0x04,0x11,0x0C,0x7F,0x03,0x11,0x0C,0x00,0x03,0x10,0x0D,0x00,0x02,0x0F,0x0E,0x01,
    0x01,0x0F,0x0F,0x01,0x01,0x0E,0x0F,0x02,0x00,0x0D,0x10,0x03,0x7F,0x0C,0x11,0x04,
    0x7F,0x0C,0x11,0x04,0x7F,0x0B,0x11,0x05,0x7E,0x0A,0x12,0x06,0x7E,0x09,0x12,0x07,
    0x40,0x02,
    0x04,0x1A,0x04,0x7E,0x02,0x1B,0x05,0x7E,0x01,0x1A,0x07,0x7E,0x00,0x1A,0x09,0x7D,
    0x7F,0x19,0x0B,0x7D,0x7E,0x18,0x0D,0x7D,0x7D,0x17,0x10,0x7C,0x7D,0x15,0x12,0x7C,
    0x7C,0x14,0x14,0x7C,0x7C,0x12,0x15,0x7D,0x7C,0x10,0x17,0x7D,0x7C,0x0D,0x18,0x7F,
    0x7D,0x0B,0x19,0x7F,0x7D,0x09,0x1A,0x00,0x7D,0x07,0x1A,0x02,0x7E,0x05,0x1B,0x02,
    0xFF,0xFF
};

static const unsigned char SiS_Part2CLVX_5[] = {   /* 750p */
    0x00,0x03,
    0x05,0x19,0x05,0x7D,0x03,0x19,0x06,0x7E,0x02,0x19,0x08,0x7D,0x01,0x18,0x0A,0x7D,
    0x00,0x18,0x0C,0x7C,0x7F,0x17,0x0E,0x7C,0x7E,0x16,0x0F,0x7D,0x7E,0x14,0x11,0x7D,
    0x7D,0x13,0x13,0x7D,0x7D,0x11,0x14,0x7E,0x7D,0x0F,0x16,0x7E,0x7D,0x0E,0x17,0x7E,
    0x7D,0x0C,0x18,0x7F,0x7D,0x0A,0x18,0x01,0x7D,0x08,0x19,0x02,0x7D,0x06,0x19,0x04,
    0xFF,0xFF
};

static const unsigned char SiS_Part2CLVX_6[] = {   /* 1080i */
    0x00,0x04,
    0x04,0x1A,0x04,0x7E,0x02,0x1B,0x05,0x7E,0x01,0x1A,0x07,0x7E,0x00,0x1A,0x09,0x7D,
    0x7F,0x19,0x0B,0x7D,0x7E,0x18,0x0D,0x7D,0x7D,0x17,0x10,0x7C,0x7D,0x15,0x12,0x7C,
    0x7C,0x14,0x14,0x7C,0x7C,0x12,0x15,0x7D,0x7C,0x10,0x17,0x7D,0x7C,0x0D,0x18,0x7F,
    0x7D,0x0B,0x19,0x7F,0x7D,0x09,0x1A,0x00,0x7D,0x07,0x1A,0x02,0x7E,0x05,0x1B,0x02,
    0xFF,0xFF,
};

#ifdef SIS315H
/* 661 et al LCD data structure (2.03.00) */
static const unsigned char SiS_LCDStruct661[] = {
    /* 1024x768 */
/*  type|CR37|   HDE   |   VDE   |    HT   |    VT   |   hss    | hse   */
    0x02,0xC0,0x00,0x04,0x00,0x03,0x40,0x05,0x26,0x03,0x10,0x00,0x88,
    0x00,0x02,0x00,0x06,0x00,0x41,0x5A,0x64,0x00,0x00,0x00,0x00,0x04,
    /*  | vss     |    vse  |clck|  clock  |CRT2DataP|CRT2DataP|idx     */
    /*					      VESA    non-VESA  noscale */
    /* 1280x1024 */
    0x03,0xC0,0x00,0x05,0x00,0x04,0x98,0x06,0x2A,0x04,0x30,0x00,0x70,
    0x00,0x01,0x00,0x03,0x00,0x6C,0xF8,0x2F,0x00,0x00,0x00,0x00,0x08,
    /* 1400x1050 */
    0x09,0x20,0x78,0x05,0x1A,0x04,0x98,0x06,0x2A,0x04,0x18,0x00,0x38,
    0x00,0x01,0x00,0x03,0x00,0x6C,0xF8,0x2F,0x00,0x00,0x00,0x00,0x09,
    /* 1600x1200 */
    0x0B,0xE0,0x40,0x06,0xB0,0x04,0x70,0x08,0xE2,0x04,0x40,0x00,0xC0,
    0x00,0x01,0x00,0x03,0x00,0xA2,0x70,0x24,0x00,0x00,0x00,0x00,0x0A,
    /* 1280x768 (_2) */
    0x0A,0xE0,0x00,0x05,0x00,0x03,0x7C,0x06,0x26,0x03,0x30,0x00,0x70,
    0x00,0x03,0x00,0x06,0x00,0x4D,0xC8,0x48,0x00,0x00,0x00,0x00,0x06,
    /* 1280x720 */
    0x0E,0xE0,0x00,0x05,0xD0,0x02,0x80,0x05,0x26,0x03,0x10,0x00,0x20,
    0x00,0x01,0x00,0x06,0x00,0x45,0x9C,0x62,0x00,0x00,0x00,0x00,0x05,
    /* 1280x800 (_2) */
    0x0C,0xE0,0x00,0x05,0x20,0x03,0x10,0x06,0x2C,0x03,0x30,0x00,0x70,
    0x00,0x04,0x00,0x03,0x00,0x49,0xCE,0x1E,0x00,0x00,0x00,0x00,0x09,
    /* 1680x1050 */
    0x0D,0xE0,0x90,0x06,0x1A,0x04,0x6C,0x07,0x2A,0x04,0x1A,0x00,0x4C,
    0x00,0x03,0x00,0x06,0x00,0x79,0xBE,0x44,0x00,0x00,0x00,0x00,0x06,
    /* 1280x800_3 */
    0x0C,0xE0,0x00,0x05,0x20,0x03,0xAA,0x05,0x2E,0x03,0x30,0x00,0x50,
    0x00,0x04,0x00,0x03,0x00,0x47,0xA9,0x10,0x00,0x00,0x00,0x00,0x07,
    /* 800x600 */
    0x01,0xC0,0x20,0x03,0x58,0x02,0x20,0x04,0x74,0x02,0x2A,0x00,0x80,
    0x00,0x06,0x00,0x04,0x00,0x28,0x63,0x4B,0x00,0x00,0x00,0x00,0x00,
    /* 1280x854 */
    0x08,0xE0,0x00,0x05,0x56,0x03,0x80,0x06,0x5d,0x03,0x10,0x00,0x70,
    0x00,0x01,0x00,0x03,0x00,0x54,0x75,0x13,0x00,0x00,0x00,0x00,0x08
};
#endif

#ifdef SIS300
static unsigned char SiS300_TrumpionData[14][80] = {
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0x7F,0x00,0x80,0x02,
    0x20,0x03,0x0B,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x10,0x00,0x00,0x04,0x23,
    0x00,0x00,0x03,0x28,0x03,0x10,0x05,0x08,0x40,0x10,0x00,0x10,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xBC,0x01,0xFF,0x03,0xFF,0x19,0x01,0x00,0x05,0x09,0x04,0x04,0x05,
    0x04,0x0C,0x09,0x05,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x5A,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0x27,0x00,0x80,0x02,
    0x20,0x03,0x07,0x00,0x5E,0x01,0x0D,0x02,0x60,0x0C,0x30,0x11,0x00,0x00,0x04,0x23,
    0x00,0x00,0x03,0x80,0x03,0x28,0x06,0x08,0x40,0x11,0x00,0x11,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0x90,0x01,0xFF,0x0F,0xF4,0x19,0x01,0x00,0x05,0x01,0x00,0x04,0x05,
    0x04,0x0C,0x02,0x01,0x02,0xB0,0x00,0x00,0x02,0xBA,0xEC,0x57,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0x8A,0x00,0xD8,0x02,
    0x84,0x03,0x16,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x1C,0x00,0x20,0x04,0x23,
    0x00,0x01,0x03,0x53,0x03,0x28,0x06,0x08,0x40,0x1C,0x00,0x16,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xD9,0x01,0xFF,0x0F,0xF4,0x18,0x07,0x05,0x05,0x13,0x04,0x04,0x05,
    0x01,0x0B,0x13,0x0A,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x59,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0x72,0x00,0xD8,0x02,
    0x84,0x03,0x16,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x1C,0x00,0x20,0x04,0x23,
    0x00,0x01,0x03,0x53,0x03,0x28,0x06,0x08,0x40,0x1C,0x00,0x16,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xDA,0x01,0xFF,0x0F,0xF4,0x18,0x07,0x05,0x05,0x13,0x04,0x04,0x05,
    0x01,0x0B,0x13,0x0A,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x55,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x02,0x00,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0x7F,0x00,0x80,0x02,
    0x20,0x03,0x16,0x00,0xE0,0x01,0x0D,0x02,0x60,0x0C,0x30,0x98,0x00,0x00,0x04,0x23,
    0x00,0x01,0x03,0x45,0x03,0x48,0x06,0x08,0x40,0x98,0x00,0x98,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xF4,0x01,0xFF,0x0F,0xF4,0x18,0x01,0x00,0x05,0x01,0x00,0x05,0x05,
    0x04,0x0C,0x08,0x05,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x5B,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x02,0x01,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0xBF,0x00,0x20,0x03,
    0x20,0x04,0x0D,0x00,0x58,0x02,0x71,0x02,0x80,0x0C,0x30,0x9A,0x00,0xFA,0x03,0x1D,
    0x00,0x01,0x03,0x22,0x03,0x28,0x06,0x08,0x40,0x98,0x00,0x98,0x04,0x1D,0x00,0x1D,
    0x03,0x11,0x60,0x39,0x03,0x40,0x05,0xF4,0x18,0x07,0x02,0x06,0x04,0x01,0x06,0x0B,
    0x02,0x0A,0x20,0x19,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x5B,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x0D,0x00,0x0D,0x10,0xEF,0x00,0x00,0x04,
    0x40,0x05,0x13,0x00,0x00,0x03,0x26,0x03,0x88,0x0C,0x30,0x90,0x00,0x00,0x04,0x23,
    0x00,0x01,0x03,0x24,0x03,0x28,0x06,0x08,0x40,0x90,0x00,0x90,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0x40,0x05,0xFF,0x0F,0xF4,0x18,0x01,0x00,0x08,0x01,0x00,0x08,0x01,
    0x00,0x08,0x01,0x01,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x5B,0x01,0xBE,0x01,0x00 },
  /* variant 2 */
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0x7F,0x00,0x80,0x02,
    0x20,0x03,0x15,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x18,0x00,0x00,0x04,0x23,
    0x00,0x01,0x03,0x44,0x03,0x28,0x06,0x08,0x40,0x18,0x00,0x18,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xA6,0x01,0xFF,0x03,0xFF,0x19,0x01,0x00,0x05,0x13,0x04,0x04,0x05,
    0x04,0x0C,0x13,0x0A,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x55,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0x7F,0x00,0x80,0x02,
    0x20,0x03,0x15,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x18,0x00,0x00,0x04,0x23,
    0x00,0x01,0x03,0x44,0x03,0x28,0x06,0x08,0x40,0x18,0x00,0x18,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xA6,0x01,0xFF,0x03,0xFF,0x19,0x01,0x00,0x05,0x13,0x04,0x04,0x05,
    0x04,0x0C,0x13,0x0A,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x55,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0x8A,0x00,0xD8,0x02,
    0x84,0x03,0x16,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x1C,0x00,0x20,0x04,0x23,
    0x00,0x01,0x03,0x53,0x03,0x28,0x06,0x08,0x40,0x1C,0x00,0x16,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xDA,0x01,0xFF,0x0F,0xF4,0x18,0x07,0x05,0x05,0x13,0x04,0x04,0x05,
    0x01,0x0B,0x13,0x0A,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x55,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0x72,0x00,0xD8,0x02,
    0x84,0x03,0x16,0x00,0x90,0x01,0xC1,0x01,0x60,0x0C,0x30,0x1C,0x00,0x20,0x04,0x23,
    0x00,0x01,0x03,0x53,0x03,0x28,0x06,0x08,0x40,0x1C,0x00,0x16,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xDA,0x01,0xFF,0x0F,0xF4,0x18,0x07,0x05,0x05,0x13,0x04,0x04,0x05,
    0x01,0x0B,0x13,0x0A,0x02,0xB0,0x00,0x00,0x02,0xBA,0xF0,0x55,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x02,0x00,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0x7F,0x00,0x80,0x02,
    0x20,0x03,0x16,0x00,0xE0,0x01,0x0D,0x02,0x60,0x0C,0x30,0x98,0x00,0x00,0x04,0x23,
    0x00,0x01,0x03,0x45,0x03,0x48,0x06,0x08,0x40,0x98,0x00,0x98,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0xF4,0x01,0xFF,0x0F,0xF4,0x18,0x01,0x00,0x05,0x01,0x00,0x05,0x05,
    0x04,0x0C,0x08,0x05,0x02,0xB0,0x00,0x00,0x02,0xBA,0xEA,0x58,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x02,0x01,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0xBF,0x00,0x20,0x03,
    0x20,0x04,0x0D,0x00,0x58,0x02,0x71,0x02,0x80,0x0C,0x30,0x9A,0x00,0xFA,0x03,0x1D,
    0x00,0x01,0x03,0x22,0x03,0x28,0x06,0x08,0x40,0x98,0x00,0x98,0x04,0x1D,0x00,0x1D,
    0x03,0x11,0x60,0x39,0x03,0x40,0x05,0xF4,0x18,0x07,0x02,0x06,0x04,0x01,0x06,0x0B,
    0x02,0x0A,0x20,0x19,0x02,0xB0,0x00,0x00,0x02,0xBA,0xEA,0x58,0x01,0xBE,0x01,0x00 },
  { 0x02,0x0A,0x0A,0x01,0x04,0x01,0x00,0x03,0x11,0x00,0x0D,0x10,0xEF,0x00,0x00,0x04,
    0x40,0x05,0x13,0x00,0x00,0x03,0x26,0x03,0x88,0x0C,0x30,0x90,0x00,0x00,0x04,0x23,
    0x00,0x01,0x03,0x24,0x03,0x28,0x06,0x08,0x40,0x90,0x00,0x90,0x04,0x23,0x00,0x23,
    0x03,0x11,0x60,0x40,0x05,0xFF,0x0F,0xF4,0x18,0x01,0x00,0x08,0x01,0x00,0x08,0x01,
    0x00,0x08,0x01,0x01,0x02,0xB0,0x00,0x00,0x02,0xBA,0xEA,0x58,0x01,0xBE,0x01,0x00 }
};
#endif

void		SiS_UnLockCRT2(struct SiS_Private *SiS_Pr);
void		SiS_EnableCRT2(struct SiS_Private *SiS_Pr);
unsigned short	SiS_GetRatePtr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex);
void		SiS_WaitRetrace1(struct SiS_Private *SiS_Pr);
bool		SiS_IsDualEdge(struct SiS_Private *SiS_Pr);
bool		SiS_IsVAMode(struct SiS_Private *SiS_Pr);
void		SiS_GetVBInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
			unsigned short ModeIdIndex, int checkcrt2mode);
void		SiS_SetYPbPr(struct SiS_Private *SiS_Pr);
void    	SiS_SetTVMode(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
			unsigned short ModeIdIndex);
void		SiS_GetLCDResInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex);
unsigned short	SiS_GetVCLK2Ptr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
			unsigned short RefreshRateTableIndex);
unsigned short	SiS_GetResInfo(struct SiS_Private *SiS_Pr,unsigned short ModeNo,unsigned short ModeIdIndex);
void		SiS_DisableBridge(struct SiS_Private *SiS_Pr);
bool		SiS_SetCRT2Group(struct SiS_Private *SiS_Pr, unsigned short ModeNo);
void		SiS_SiS30xBLOn(struct SiS_Private *SiS_Pr);
void		SiS_SiS30xBLOff(struct SiS_Private *SiS_Pr);

void		SiS_SetCH700x(struct SiS_Private *SiS_Pr, unsigned short reg, unsigned char val);
unsigned short	SiS_GetCH700x(struct SiS_Private *SiS_Pr, unsigned short tempax);
void		SiS_SetCH701x(struct SiS_Private *SiS_Pr, unsigned short reg, unsigned char val);
unsigned short	SiS_GetCH701x(struct SiS_Private *SiS_Pr, unsigned short tempax);
void		SiS_SetCH70xxANDOR(struct SiS_Private *SiS_Pr, unsigned short reg,
			unsigned char orval,unsigned short andval);
#ifdef SIS315H
static void	SiS_Chrontel701xOn(struct SiS_Private *SiS_Pr);
static void	SiS_Chrontel701xOff(struct SiS_Private *SiS_Pr);
static void	SiS_ChrontelInitTVVSync(struct SiS_Private *SiS_Pr);
static void	SiS_ChrontelDoSomething1(struct SiS_Private *SiS_Pr);
void		SiS_Chrontel701xBLOn(struct SiS_Private *SiS_Pr);
void		SiS_Chrontel701xBLOff(struct SiS_Private *SiS_Pr);
#endif /* 315 */

#ifdef SIS300
static  bool	SiS_SetTrumpionBlock(struct SiS_Private *SiS_Pr, unsigned char *dataptr);
void		SiS_SetChrontelGPIO(struct SiS_Private *SiS_Pr, unsigned short myvbinfo);
#endif

void		SiS_DDC2Delay(struct SiS_Private *SiS_Pr, unsigned int delaytime);
unsigned short	SiS_ReadDDC1Bit(struct SiS_Private *SiS_Pr);
unsigned short	SiS_HandleDDC(struct SiS_Private *SiS_Pr, unsigned int VBFlags, int VGAEngine,
			unsigned short adaptnum, unsigned short DDCdatatype,
			unsigned char *buffer, unsigned int VBFlags2);

static unsigned short	SiS_InitDDCRegs(struct SiS_Private *SiS_Pr, unsigned int VBFlags,
				int VGAEngine, unsigned short adaptnum, unsigned short DDCdatatype,
				bool checkcr32, unsigned int VBFlags2);
static unsigned short	SiS_ProbeDDC(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_ReadDDC(struct SiS_Private *SiS_Pr, unsigned short DDCdatatype,
				unsigned char *buffer);
static void		SiS_SetSwitchDDC2(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_SetStart(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_SetStop(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_SetSCLKLow(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_SetSCLKHigh(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_ReadDDC2Data(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_WriteDDC2Data(struct SiS_Private *SiS_Pr, unsigned short tempax);
static unsigned short	SiS_CheckACK(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_WriteDABDDC(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_PrepareReadDDC(struct SiS_Private *SiS_Pr);
static unsigned short	SiS_PrepareDDC(struct SiS_Private *SiS_Pr);
static void		SiS_SendACK(struct SiS_Private *SiS_Pr, unsigned short yesno);
static unsigned short	SiS_DoProbeDDC(struct SiS_Private *SiS_Pr);

#ifdef SIS300
static void		SiS_OEM300Setting(struct SiS_Private *SiS_Pr,
				unsigned short ModeNo, unsigned short ModeIdIndex, unsigned short RefTabindex);
static void		SetOEMLCDData2(struct SiS_Private *SiS_Pr,
				unsigned short ModeNo, unsigned short ModeIdIndex,unsigned short RefTableIndex);
#endif
#ifdef SIS315H
static void		SiS_OEM310Setting(struct SiS_Private *SiS_Pr,
				unsigned short ModeNo,unsigned short ModeIdIndex, unsigned short RRTI);
static void		SiS_OEM661Setting(struct SiS_Private *SiS_Pr,
				unsigned short ModeNo,unsigned short ModeIdIndex, unsigned short RRTI);
static void		SiS_FinalizeLCD(struct SiS_Private *, unsigned short, unsigned short);
#endif

extern void		SiS_SetReg(SISIOADDRESS, unsigned short, unsigned short);
extern void		SiS_SetRegByte(SISIOADDRESS, unsigned short);
extern void		SiS_SetRegShort(SISIOADDRESS, unsigned short);
extern void		SiS_SetRegLong(SISIOADDRESS, unsigned int);
extern unsigned char	SiS_GetReg(SISIOADDRESS, unsigned short);
extern unsigned char	SiS_GetRegByte(SISIOADDRESS);
extern unsigned short	SiS_GetRegShort(SISIOADDRESS);
extern unsigned int	SiS_GetRegLong(SISIOADDRESS);
extern void		SiS_SetRegANDOR(SISIOADDRESS, unsigned short, unsigned short, unsigned short);
extern void		SiS_SetRegOR(SISIOADDRESS, unsigned short, unsigned short);
extern void		SiS_SetRegAND(SISIOADDRESS, unsigned short, unsigned short);
extern void		SiS_DisplayOff(struct SiS_Private *SiS_Pr);
extern void		SiS_DisplayOn(struct SiS_Private *SiS_Pr);
extern bool		SiS_SearchModeID(struct SiS_Private *, unsigned short *, unsigned short *);
extern unsigned short	SiS_GetModeFlag(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
				unsigned short ModeIdIndex);
extern unsigned short	SiS_GetModePtr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex);
extern unsigned short	SiS_GetColorDepth(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex);
extern unsigned short	SiS_GetOffset(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
				unsigned short RefreshRateTableIndex);
extern void		SiS_LoadDAC(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
				unsigned short ModeIdIndex);
extern void		SiS_CalcLCDACRT1Timing(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
				unsigned short ModeIdIndex);
extern void		SiS_CalcCRRegisters(struct SiS_Private *SiS_Pr, int depth);
extern unsigned short	SiS_GetRefCRTVCLK(struct SiS_Private *SiS_Pr, unsigned short Index, int UseWide);
extern unsigned short	SiS_GetRefCRT1CRTC(struct SiS_Private *SiS_Pr, unsigned short Index, int UseWide);
#ifdef SIS300
extern void		SiS_GetFIFOThresholdIndex300(struct SiS_Private *SiS_Pr, unsigned short *tempbx,
				unsigned short *tempcl);
extern unsigned short	SiS_GetFIFOThresholdB300(unsigned short tempbx, unsigned short tempcl);
extern unsigned short	SiS_GetLatencyFactor630(struct SiS_Private *SiS_Pr, unsigned short index);
extern unsigned int	sisfb_read_nbridge_pci_dword(struct SiS_Private *SiS_Pr, int reg);
extern unsigned int	sisfb_read_lpc_pci_dword(struct SiS_Private *SiS_Pr, int reg);
#endif

#endif
