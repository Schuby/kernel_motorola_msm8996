/*
 * ALC660-VD/ALC861-VD quirk models
 * included by patch_realtek.c
 */

/* ALC861-VD models */
enum {
	ALC861VD_AUTO,
	ALC660VD_3ST,
	ALC660VD_3ST_DIG,
	ALC861VD_3ST,
	ALC861VD_3ST_DIG,
	ALC861VD_6ST_DIG,
	ALC861VD_MODEL_LAST,
};

#define ALC861VD_DIGOUT_NID	0x06

static const hda_nid_t alc861vd_dac_nids[4] = {
	/* front, surr, clfe, side surr */
	0x02, 0x03, 0x04, 0x05
};

/* dac_nids for ALC660vd are in a different order - according to
 * Realtek's driver.
 * This should probably result in a different mixer for 6stack models
 * of ALC660vd codecs, but for now there is only 3stack mixer
 * - and it is the same as in 861vd.
 * adc_nids in ALC660vd are (is) the same as in 861vd
 */
static const hda_nid_t alc660vd_dac_nids[3] = {
	/* front, rear, clfe, rear_surr */
	0x02, 0x04, 0x03
};

static const hda_nid_t alc861vd_adc_nids[1] = {
	/* ADC0 */
	0x09,
};

static const hda_nid_t alc861vd_capsrc_nids[1] = { 0x22 };

/* input MUX */
/* FIXME: should be a matrix-type input source selection */
static const struct hda_input_mux alc861vd_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x1 },
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/*
 * 2ch mode
 */
static const struct hda_channel_mode alc861vd_3stack_2ch_modes[1] = {
	{ 2, NULL }
};

/*
 * 6ch mode
 */
static const struct hda_verb alc861vd_6stack_ch6_init[] = {
	{ 0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x00 },
	{ 0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ } /* end */
};

/*
 * 8ch mode
 */
static const struct hda_verb alc861vd_6stack_ch8_init[] = {
	{ 0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ } /* end */
};

static const struct hda_channel_mode alc861vd_6stack_modes[2] = {
	{ 6, alc861vd_6stack_ch6_init },
	{ 8, alc861vd_6stack_ch8_init },
};

static const struct snd_kcontrol_new alc861vd_chmode_mixer[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

/* Pin assignment: Front=0x14, Rear=0x15, CLFE=0x16, Side=0x17
 *                 Mic=0x18, Front Mic=0x19, Line-In=0x1a, HP=0x1b
 */
static const struct snd_kcontrol_new alc861vd_6st_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x02, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),

	HDA_CODEC_VOLUME("Surround Playback Volume", 0x03, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),

	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x04, 1, 0x0,
				HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x04, 2, 0x0,
				HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),

	HDA_CODEC_VOLUME("Side Playback Volume", 0x05, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0f, 2, HDA_INPUT),

	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1b, 0x0, HDA_OUTPUT),

	HDA_CODEC_VOLUME("Mic Boost Volume", 0x18, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),

	HDA_CODEC_VOLUME("Front Mic Boost Volume", 0x19, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),

	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),

	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),

	{ } /* end */
};

static const struct snd_kcontrol_new alc861vd_3st_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x02, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),

	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1b, 0x0, HDA_OUTPUT),

	HDA_CODEC_VOLUME("Mic Boost Volume", 0x18, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),

	HDA_CODEC_VOLUME("Front Mic Boost Volume", 0x19, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),

	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),

	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),

	{ } /* end */
};

static const struct snd_kcontrol_new alc861vd_lenovo_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x02, 0x0, HDA_OUTPUT),
	/*HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),*/
	HDA_CODEC_MUTE("Front Playback Switch", 0x14, 0x0, HDA_OUTPUT),

	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1b, 0x0, HDA_OUTPUT),

	HDA_CODEC_VOLUME("Mic Boost Volume", 0x18, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),

	HDA_CODEC_VOLUME("Front Mic Boost Volume", 0x19, 0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),

	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),

	{ } /* end */
};

/*
 * generic initialization of ADC, input mixers and output mixers
 */
static const struct hda_verb alc861vd_volume_init_verbs[] = {
	/*
	 * Unmute ADC0 and set the default input to mic-in
	 */
	{0x09, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x09, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},

	/* Unmute input amps (CD, Line In, Mic 1 & Mic 2) of
	 * the analog-loopback mixer widget
	 */
	/* Amp Indices: Mic1 = 0, Mic2 = 1, Line1 = 2, Line2 = 3, CD = 4 */
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},

	/* Capture mixer: unmute Mic, F-Mic, Line, CD inputs */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(2)},
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(4)},

	/*
	 * Set up output mixers (0x02 - 0x05)
	 */
	/* set vol=0 to output mixers */
	{0x02, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x03, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x04, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},

	/* set up input amps for analog loopback */
	/* Amp Indices: DAC = 0, mixer = 1 */
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},

	{ }
};

/*
 * 3-stack pin configuration:
 * front = 0x14, mic/clfe = 0x18, HP = 0x19, line/surr = 0x1a, f-mic = 0x1b
 */
static const struct hda_verb alc861vd_3stack_init_verbs[] = {
	/*
	 * Set pin mode and muting
	 */
	/* set front pin widgets 0x14 for output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},

	/* Mic (rear) pin: input vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Front Mic pin: input vref at 80% */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Line In pin: input */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Line-2 In: Headphone output (output 0 - 0x0c) */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},
	/* CD pin widget for input */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * 6-stack pin configuration:
 */
static const struct hda_verb alc861vd_6stack_init_verbs[] = {
	/*
	 * Set pin mode and muting
	 */
	/* set front pin widgets 0x14 for output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},

	/* Rear Pin: output 1 (0x0d) */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x01},
	/* CLFE Pin: output 2 (0x0e) */
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_CONNECT_SEL, 0x02},
	/* Side Pin: output 3 (0x0f) */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},

	/* Mic (rear) pin: input vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Front Mic pin: input vref at 80% */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Line In pin: input */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Line-2 In: Headphone output (output 0 - 0x0c) */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},
	/* CD pin widget for input */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

static const struct hda_verb alc861vd_eapd_verbs[] = {
	{0x14, AC_VERB_SET_EAPD_BTLENABLE, 2},
	{ }
};

/*
 * configuration and preset
 */
static const char * const alc861vd_models[ALC861VD_MODEL_LAST] = {
	[ALC660VD_3ST]		= "3stack-660",
	[ALC660VD_3ST_DIG]	= "3stack-660-digout",
	[ALC861VD_3ST]		= "3stack",
	[ALC861VD_3ST_DIG]	= "3stack-digout",
	[ALC861VD_6ST_DIG]	= "6stack-digout",
	[ALC861VD_AUTO]		= "auto",
};

static const struct snd_pci_quirk alc861vd_cfg_tbl[] = {
	SND_PCI_QUIRK(0x1019, 0xa88d, "Realtek ALC660 demo", ALC660VD_3ST),
	SND_PCI_QUIRK(0x10de, 0x03f0, "Realtek ALC660 demo", ALC660VD_3ST),
	SND_PCI_QUIRK(0x1565, 0x820d, "Biostar NF61S SE", ALC861VD_6ST_DIG),
	SND_PCI_QUIRK(0x1849, 0x0862, "ASRock K8NF6G-VSTA", ALC861VD_6ST_DIG),
	{}
};

static const struct alc_config_preset alc861vd_presets[] = {
	[ALC660VD_3ST] = {
		.mixers = { alc861vd_3st_mixer },
		.init_verbs = { alc861vd_volume_init_verbs,
				 alc861vd_3stack_init_verbs },
		.num_dacs = ARRAY_SIZE(alc660vd_dac_nids),
		.dac_nids = alc660vd_dac_nids,
		.num_channel_mode = ARRAY_SIZE(alc861vd_3stack_2ch_modes),
		.channel_mode = alc861vd_3stack_2ch_modes,
		.input_mux = &alc861vd_capture_source,
	},
	[ALC660VD_3ST_DIG] = {
		.mixers = { alc861vd_3st_mixer },
		.init_verbs = { alc861vd_volume_init_verbs,
				 alc861vd_3stack_init_verbs },
		.num_dacs = ARRAY_SIZE(alc660vd_dac_nids),
		.dac_nids = alc660vd_dac_nids,
		.dig_out_nid = ALC861VD_DIGOUT_NID,
		.num_channel_mode = ARRAY_SIZE(alc861vd_3stack_2ch_modes),
		.channel_mode = alc861vd_3stack_2ch_modes,
		.input_mux = &alc861vd_capture_source,
	},
	[ALC861VD_3ST] = {
		.mixers = { alc861vd_3st_mixer },
		.init_verbs = { alc861vd_volume_init_verbs,
				 alc861vd_3stack_init_verbs },
		.num_dacs = ARRAY_SIZE(alc861vd_dac_nids),
		.dac_nids = alc861vd_dac_nids,
		.num_channel_mode = ARRAY_SIZE(alc861vd_3stack_2ch_modes),
		.channel_mode = alc861vd_3stack_2ch_modes,
		.input_mux = &alc861vd_capture_source,
	},
	[ALC861VD_3ST_DIG] = {
		.mixers = { alc861vd_3st_mixer },
		.init_verbs = { alc861vd_volume_init_verbs,
		 		 alc861vd_3stack_init_verbs },
		.num_dacs = ARRAY_SIZE(alc861vd_dac_nids),
		.dac_nids = alc861vd_dac_nids,
		.dig_out_nid = ALC861VD_DIGOUT_NID,
		.num_channel_mode = ARRAY_SIZE(alc861vd_3stack_2ch_modes),
		.channel_mode = alc861vd_3stack_2ch_modes,
		.input_mux = &alc861vd_capture_source,
	},
	[ALC861VD_6ST_DIG] = {
		.mixers = { alc861vd_6st_mixer, alc861vd_chmode_mixer },
		.init_verbs = { alc861vd_volume_init_verbs,
				alc861vd_6stack_init_verbs },
		.num_dacs = ARRAY_SIZE(alc861vd_dac_nids),
		.dac_nids = alc861vd_dac_nids,
		.dig_out_nid = ALC861VD_DIGOUT_NID,
		.num_channel_mode = ARRAY_SIZE(alc861vd_6stack_modes),
		.channel_mode = alc861vd_6stack_modes,
		.input_mux = &alc861vd_capture_source,
	},
};

