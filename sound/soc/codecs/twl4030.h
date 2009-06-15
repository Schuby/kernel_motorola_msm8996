/*
 * ALSA SoC TWL4030 codec driver
 *
 * Author: Steve Sakoman <steve@sakoman.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TWL4030_AUDIO_H__
#define __TWL4030_AUDIO_H__

#define TWL4030_REG_CODEC_MODE		0x1
#define TWL4030_REG_OPTION		0x2
#define TWL4030_REG_UNKNOWN		0x3
#define TWL4030_REG_MICBIAS_CTL		0x4
#define TWL4030_REG_ANAMICL		0x5
#define TWL4030_REG_ANAMICR		0x6
#define TWL4030_REG_AVADC_CTL		0x7
#define TWL4030_REG_ADCMICSEL		0x8
#define TWL4030_REG_DIGMIXING		0x9
#define TWL4030_REG_ATXL1PGA		0xA
#define TWL4030_REG_ATXR1PGA		0xB
#define TWL4030_REG_AVTXL2PGA		0xC
#define TWL4030_REG_AVTXR2PGA		0xD
#define TWL4030_REG_AUDIO_IF		0xE
#define TWL4030_REG_VOICE_IF		0xF
#define TWL4030_REG_ARXR1PGA		0x10
#define TWL4030_REG_ARXL1PGA		0x11
#define TWL4030_REG_ARXR2PGA		0x12
#define TWL4030_REG_ARXL2PGA		0x13
#define TWL4030_REG_VRXPGA		0x14
#define TWL4030_REG_VSTPGA		0x15
#define TWL4030_REG_VRX2ARXPGA		0x16
#define TWL4030_REG_AVDAC_CTL		0x17
#define TWL4030_REG_ARX2VTXPGA		0x18
#define TWL4030_REG_ARXL1_APGA_CTL	0x19
#define TWL4030_REG_ARXR1_APGA_CTL	0x1A
#define TWL4030_REG_ARXL2_APGA_CTL	0x1B
#define TWL4030_REG_ARXR2_APGA_CTL	0x1C
#define TWL4030_REG_ATX2ARXPGA		0x1D
#define TWL4030_REG_BT_IF		0x1E
#define TWL4030_REG_BTPGA		0x1F
#define TWL4030_REG_BTSTPGA		0x20
#define TWL4030_REG_EAR_CTL		0x21
#define TWL4030_REG_HS_SEL		0x22
#define TWL4030_REG_HS_GAIN_SET		0x23
#define TWL4030_REG_HS_POPN_SET		0x24
#define TWL4030_REG_PREDL_CTL		0x25
#define TWL4030_REG_PREDR_CTL		0x26
#define TWL4030_REG_PRECKL_CTL		0x27
#define TWL4030_REG_PRECKR_CTL		0x28
#define TWL4030_REG_HFL_CTL		0x29
#define TWL4030_REG_HFR_CTL		0x2A
#define TWL4030_REG_ALC_CTL		0x2B
#define TWL4030_REG_ALC_SET1		0x2C
#define TWL4030_REG_ALC_SET2		0x2D
#define TWL4030_REG_BOOST_CTL		0x2E
#define TWL4030_REG_SOFTVOL_CTL		0x2F
#define TWL4030_REG_DTMF_FREQSEL	0x30
#define TWL4030_REG_DTMF_TONEXT1H	0x31
#define TWL4030_REG_DTMF_TONEXT1L	0x32
#define TWL4030_REG_DTMF_TONEXT2H	0x33
#define TWL4030_REG_DTMF_TONEXT2L	0x34
#define TWL4030_REG_DTMF_TONOFF		0x35
#define TWL4030_REG_DTMF_WANONOFF	0x36
#define TWL4030_REG_I2S_RX_SCRAMBLE_H	0x37
#define TWL4030_REG_I2S_RX_SCRAMBLE_M	0x38
#define TWL4030_REG_I2S_RX_SCRAMBLE_L	0x39
#define TWL4030_REG_APLL_CTL		0x3A
#define TWL4030_REG_DTMF_CTL		0x3B
#define TWL4030_REG_DTMF_PGA_CTL2	0x3C
#define TWL4030_REG_DTMF_PGA_CTL1	0x3D
#define TWL4030_REG_MISC_SET_1		0x3E
#define TWL4030_REG_PCMBTMUX		0x3F
#define TWL4030_REG_RX_PATH_SEL		0x43
#define TWL4030_REG_VDL_APGA_CTL	0x44
#define TWL4030_REG_VIBRA_CTL		0x45
#define TWL4030_REG_VIBRA_SET		0x46
#define TWL4030_REG_VIBRA_PWM_SET	0x47
#define TWL4030_REG_ANAMIC_GAIN		0x48
#define TWL4030_REG_MISC_SET_2		0x49
#define TWL4030_REG_SW_SHADOW		0x4A

#define TWL4030_CACHEREGNUM	(TWL4030_REG_SW_SHADOW + 1)

/* Bitfield Definitions */

/* TWL4030_CODEC_MODE (0x01) Fields */

#define TWL4030_APLL_RATE		0xF0
#define TWL4030_APLL_RATE_8000		0x00
#define TWL4030_APLL_RATE_11025		0x10
#define TWL4030_APLL_RATE_12000		0x20
#define TWL4030_APLL_RATE_16000		0x40
#define TWL4030_APLL_RATE_22050		0x50
#define TWL4030_APLL_RATE_24000		0x60
#define TWL4030_APLL_RATE_32000		0x80
#define TWL4030_APLL_RATE_44100		0x90
#define TWL4030_APLL_RATE_48000		0xA0
#define TWL4030_APLL_RATE_96000		0xE0
#define TWL4030_SEL_16K			0x08
#define TWL4030_CODECPDZ		0x02
#define TWL4030_OPT_MODE		0x01
#define TWL4030_OPTION_1		(1 << 0)
#define TWL4030_OPTION_2		(0 << 0)

/* TWL4030_OPTION (0x02) Fields */

#define TWL4030_ATXL1_EN		(1 << 0)
#define TWL4030_ATXR1_EN		(1 << 1)
#define TWL4030_ATXL2_VTXL_EN		(1 << 2)
#define TWL4030_ATXR2_VTXR_EN		(1 << 3)
#define TWL4030_ARXL1_VRX_EN		(1 << 4)
#define TWL4030_ARXR1_EN		(1 << 5)
#define TWL4030_ARXL2_EN		(1 << 6)
#define TWL4030_ARXR2_EN		(1 << 7)

/* TWL4030_REG_MICBIAS_CTL (0x04) Fields */

#define TWL4030_MICBIAS2_CTL		0x40
#define TWL4030_MICBIAS1_CTL		0x20
#define TWL4030_HSMICBIAS_EN		0x04
#define TWL4030_MICBIAS2_EN		0x02
#define TWL4030_MICBIAS1_EN		0x01

/* ANAMICL (0x05) Fields */

#define TWL4030_CNCL_OFFSET_START	0x80
#define TWL4030_OFFSET_CNCL_SEL		0x60
#define TWL4030_OFFSET_CNCL_SEL_ARX1	0x00
#define TWL4030_OFFSET_CNCL_SEL_ARX2	0x20
#define TWL4030_OFFSET_CNCL_SEL_VRX	0x40
#define TWL4030_OFFSET_CNCL_SEL_ALL	0x60
#define TWL4030_MICAMPL_EN		0x10
#define TWL4030_CKMIC_EN		0x08
#define TWL4030_AUXL_EN			0x04
#define TWL4030_HSMIC_EN		0x02
#define TWL4030_MAINMIC_EN		0x01

/* ANAMICR (0x06) Fields */

#define TWL4030_MICAMPR_EN		0x10
#define TWL4030_AUXR_EN			0x04
#define TWL4030_SUBMIC_EN		0x01

/* AVADC_CTL (0x07) Fields */

#define TWL4030_ADCL_EN			0x08
#define TWL4030_AVADC_CLK_PRIORITY	0x04
#define TWL4030_ADCR_EN			0x02

/* TWL4030_REG_ADCMICSEL (0x08) Fields */

#define TWL4030_DIGMIC1_EN		0x08
#define TWL4030_TX2IN_SEL		0x04
#define TWL4030_DIGMIC0_EN		0x02
#define TWL4030_TX1IN_SEL		0x01

/* AUDIO_IF (0x0E) Fields */

#define TWL4030_AIF_SLAVE_EN		0x80
#define TWL4030_DATA_WIDTH		0x60
#define TWL4030_DATA_WIDTH_16S_16W	0x00
#define TWL4030_DATA_WIDTH_32S_16W	0x40
#define TWL4030_DATA_WIDTH_32S_24W	0x60
#define TWL4030_AIF_FORMAT		0x18
#define TWL4030_AIF_FORMAT_CODEC	0x00
#define TWL4030_AIF_FORMAT_LEFT		0x08
#define TWL4030_AIF_FORMAT_RIGHT	0x10
#define TWL4030_AIF_FORMAT_TDM		0x18
#define TWL4030_AIF_TRI_EN		0x04
#define TWL4030_CLK256FS_EN		0x02
#define TWL4030_AIF_EN			0x01

/* VOICE_IF (0x0F) Fields */

#define TWL4030_VIF_SLAVE_EN		0x80
#define TWL4030_VIF_DIN_EN		0x40
#define TWL4030_VIF_DOUT_EN		0x20
#define TWL4030_VIF_SWAP		0x10
#define TWL4030_VIF_FORMAT		0x08
#define TWL4030_VIF_TRI_EN		0x04
#define TWL4030_VIF_SUB_EN		0x02
#define TWL4030_VIF_EN			0x01

/* EAR_CTL (0x21) */
#define TWL4030_EAR_GAIN		0x30

/* HS_GAIN_SET (0x23) Fields */

#define TWL4030_HSR_GAIN		0x0C
#define TWL4030_HSR_GAIN_PWR_DOWN	0x00
#define TWL4030_HSR_GAIN_PLUS_6DB	0x04
#define TWL4030_HSR_GAIN_0DB		0x08
#define TWL4030_HSR_GAIN_MINUS_6DB	0x0C
#define TWL4030_HSL_GAIN		0x03
#define TWL4030_HSL_GAIN_PWR_DOWN	0x00
#define TWL4030_HSL_GAIN_PLUS_6DB	0x01
#define TWL4030_HSL_GAIN_0DB		0x02
#define TWL4030_HSL_GAIN_MINUS_6DB	0x03

/* HS_POPN_SET (0x24) Fields */

#define TWL4030_VMID_EN			0x40
#define	TWL4030_EXTMUTE			0x20
#define TWL4030_RAMP_DELAY		0x1C
#define TWL4030_RAMP_DELAY_20MS		0x00
#define TWL4030_RAMP_DELAY_40MS		0x04
#define TWL4030_RAMP_DELAY_81MS		0x08
#define TWL4030_RAMP_DELAY_161MS	0x0C
#define TWL4030_RAMP_DELAY_323MS	0x10
#define TWL4030_RAMP_DELAY_645MS	0x14
#define TWL4030_RAMP_DELAY_1291MS	0x18
#define TWL4030_RAMP_DELAY_2581MS	0x1C
#define TWL4030_RAMP_EN			0x02

/* PREDL_CTL (0x25) */
#define TWL4030_PREDL_GAIN		0x30

/* PREDR_CTL (0x26) */
#define TWL4030_PREDR_GAIN		0x30

/* PRECKL_CTL (0x27) */
#define TWL4030_PRECKL_GAIN		0x30

/* PRECKR_CTL (0x28) */
#define TWL4030_PRECKR_GAIN		0x30

/* HFL_CTL (0x29, 0x2A) Fields */
#define TWL4030_HF_CTL_HB_EN		0x04
#define TWL4030_HF_CTL_LOOP_EN		0x08
#define TWL4030_HF_CTL_RAMP_EN		0x10
#define TWL4030_HF_CTL_REF_EN		0x20

/* APLL_CTL (0x3A) Fields */

#define TWL4030_APLL_EN			0x10
#define TWL4030_APLL_INFREQ		0x0F
#define TWL4030_APLL_INFREQ_19200KHZ	0x05
#define TWL4030_APLL_INFREQ_26000KHZ	0x06
#define TWL4030_APLL_INFREQ_38400KHZ	0x0F

/* REG_MISC_SET_1 (0x3E) Fields */

#define TWL4030_CLK64_EN		0x80
#define TWL4030_SCRAMBLE_EN		0x40
#define TWL4030_FMLOOP_EN		0x20
#define TWL4030_SMOOTH_ANAVOL_EN	0x02
#define TWL4030_DIGMIC_LR_SWAP_EN	0x01

/* TWL4030_REG_SW_SHADOW (0x4A) Fields */
#define TWL4030_HFL_EN			0x01
#define TWL4030_HFR_EN			0x02

#define TWL4030_DAI_HIFI		0
#define TWL4030_DAI_VOICE		1

extern struct snd_soc_dai twl4030_dai[2];
extern struct snd_soc_codec_device soc_codec_dev_twl4030;

struct twl4030_setup_data {
	unsigned int ramp_delay_value;
	unsigned int sysclk;
};

#endif	/* End of __TWL4030_AUDIO_H__ */
