/*
 * Copyright (c) 2008 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include "ath9k.h"
#include "regd_common.h"

/*
 * This is a set of common rules used by our world regulatory domains.
 * We have 12 world regulatory domains. To save space we consolidate
 * the regulatory domains in 5 structures by frequency and change
 * the flags on our reg_notifier() on a case by case basis.
 */

/* Only these channels all allow active scan on all world regulatory domains */
#define ATH9K_2GHZ_CH01_11	REG_RULE(2412-10, 2462+10, 40, 0, 20, 0)

/* We enable active scan on these a case by case basis by regulatory domain */
#define ATH9K_2GHZ_CH12_13	REG_RULE(2467-10, 2472+10, 40, 0, 20,\
					NL80211_RRF_PASSIVE_SCAN)
#define ATH9K_2GHZ_CH14		REG_RULE(2484-10, 2484+10, 40, 0, 20,\
				NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_OFDM)

/* We allow IBSS on these on a case by case basis by regulatory domain */
#define ATH9K_5GHZ_5150_5350	REG_RULE(5150-10, 5350+10, 40, 0, 30,\
				NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)
#define ATH9K_5GHZ_5470_5850	REG_RULE(5470-10, 5850+10, 40, 0, 30,\
				NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)
#define ATH9K_5GHZ_5725_5850	REG_RULE(5725-10, 5850+10, 40, 0, 30,\
				NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define ATH9K_2GHZ_ALL		ATH9K_2GHZ_CH01_11, \
				ATH9K_2GHZ_CH12_13, \
				ATH9K_2GHZ_CH14

#define ATH9K_5GHZ_ALL		ATH9K_5GHZ_5150_5350, \
				ATH9K_5GHZ_5470_5850
/* This one skips what we call "mid band" */
#define ATH9K_5GHZ_NO_MIDBAND	ATH9K_5GHZ_5150_5350, \
				ATH9K_5GHZ_5725_5850

/* Can be used for:
 * 0x60, 0x61, 0x62 */
static const struct ieee80211_regdomain ath9k_world_regdom_60_61_62 = {
	.n_reg_rules = 5,
	.alpha2 =  "99",
	.reg_rules = {
		ATH9K_2GHZ_ALL,
		ATH9K_5GHZ_ALL,
	}
};

/* Can be used by 0x63 and 0x65 */
static const struct ieee80211_regdomain ath9k_world_regdom_63_65 = {
	.n_reg_rules = 4,
	.alpha2 =  "99",
	.reg_rules = {
		ATH9K_2GHZ_CH01_11,
		ATH9K_2GHZ_CH12_13,
		ATH9K_5GHZ_NO_MIDBAND,
	}
};

/* Can be used by 0x64 only */
static const struct ieee80211_regdomain ath9k_world_regdom_64 = {
	.n_reg_rules = 3,
	.alpha2 =  "99",
	.reg_rules = {
		ATH9K_2GHZ_CH01_11,
		ATH9K_5GHZ_NO_MIDBAND,
	}
};

/* Can be used by 0x66 and 0x69 */
static const struct ieee80211_regdomain ath9k_world_regdom_66_69 = {
	.n_reg_rules = 3,
	.alpha2 =  "99",
	.reg_rules = {
		ATH9K_2GHZ_CH01_11,
		ATH9K_5GHZ_ALL,
	}
};

/* Can be used by 0x67, 0x6A and 0x68 */
static const struct ieee80211_regdomain ath9k_world_regdom_67_68_6A = {
	.n_reg_rules = 4,
	.alpha2 =  "99",
	.reg_rules = {
		ATH9K_2GHZ_CH01_11,
		ATH9K_2GHZ_CH12_13,
		ATH9K_5GHZ_ALL,
	}
};

static u16 ath9k_regd_get_eepromRD(struct ath_hal *ah)
{
	return ah->ah_currentRD & ~WORLDWIDE_ROAMING_FLAG;
}

u16 ath9k_regd_get_rd(struct ath_hal *ah)
{
	return ath9k_regd_get_eepromRD(ah);
}

bool ath9k_is_world_regd(struct ath_hal *ah)
{
	return isWwrSKU(ah);
}

const struct ieee80211_regdomain *ath9k_default_world_regdomain(void)
{
	/* this is the most restrictive */
	return &ath9k_world_regdom_64;
}

const struct ieee80211_regdomain *ath9k_world_regdomain(struct ath_hal *ah)
{
	switch (ah->regpair->regDmnEnum) {
	case 0x60:
	case 0x61:
	case 0x62:
		return &ath9k_world_regdom_60_61_62;
	case 0x63:
	case 0x65:
		return &ath9k_world_regdom_63_65;
	case 0x64:
		return &ath9k_world_regdom_64;
	case 0x66:
	case 0x69:
		return &ath9k_world_regdom_66_69;
	case 0x67:
	case 0x68:
	case 0x6A:
		return &ath9k_world_regdom_67_68_6A;
	default:
		WARN_ON(1);
		return ath9k_default_world_regdomain();
	}
}

/* Frequency is one where radar detection is required */
static bool ath9k_is_radar_freq(u16 center_freq)
{
	return (center_freq >= 5260 && center_freq <= 5700);
}

/*
 * Enable adhoc on 5 GHz if allowed by 11d.
 * Remove passive scan if channel is allowed by 11d,
 * except when on radar frequencies.
 */
static void ath9k_reg_apply_5ghz_beaconing_flags(struct wiphy *wiphy,
					     enum reg_set_by setby)
{
	struct ieee80211_supported_band *sband;
	const struct ieee80211_reg_rule *reg_rule;
	struct ieee80211_channel *ch;
	unsigned int i;
	u32 bandwidth = 0;
	int r;

	if (setby != REGDOM_SET_BY_COUNTRY_IE)
		return;
	if (!wiphy->bands[IEEE80211_BAND_5GHZ])
		return;

	sband = wiphy->bands[IEEE80211_BAND_5GHZ];
	for (i = 0; i < sband->n_channels; i++) {
		ch = &sband->channels[i];
		r = freq_reg_info(wiphy, ch->center_freq,
			&bandwidth, &reg_rule);
		if (r)
			continue;
		/* If 11d had a rule for this channel ensure we enable adhoc
		 * if it allows us to use it. Note that we would have disabled
		 * it by applying our static world regdomain by default during
		 * probe */
		if (!(reg_rule->flags & NL80211_RRF_NO_IBSS))
			ch->flags &= ~IEEE80211_CHAN_NO_IBSS;
		if (!ath9k_is_radar_freq(ch->center_freq))
			continue;
		if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
			ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
	}
}

/* Allows active scan scan on Ch 12 and 13 */
static void ath9k_reg_apply_active_scan_flags(struct wiphy *wiphy,
					      enum reg_set_by setby)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *ch;
	const struct ieee80211_reg_rule *reg_rule;
	u32 bandwidth = 0;
	int r;

	/* Force passive scan on Channels 12-13 */
	sband = wiphy->bands[IEEE80211_BAND_2GHZ];

	/* If no country IE has been received always enable active scan
	 * on these channels */
	if (setby != REGDOM_SET_BY_COUNTRY_IE) {
		ch = &sband->channels[11]; /* CH 12 */
		if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
			ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
		ch = &sband->channels[12]; /* CH 13 */
		if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
			ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
		return;
	}

	/* If a country IE has been recieved check its rule for this
	 * channel first before enabling active scan. The passive scan
	 * would have been enforced by the initial probe processing on
	 * our custom regulatory domain. */

	ch = &sband->channels[11]; /* CH 12 */
	r = freq_reg_info(wiphy, ch->center_freq, &bandwidth, &reg_rule);
	if (!r) {
		if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
			if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
				ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
	}

	ch = &sband->channels[12]; /* CH 13 */
	r = freq_reg_info(wiphy, ch->center_freq, &bandwidth, &reg_rule);
	if (!r) {
		if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
			if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
				ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
	}
}

/* Always apply Radar/DFS rules on freq range 5260 MHz - 5700 MHz */
void ath9k_reg_apply_radar_flags(struct wiphy *wiphy)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *ch;
	unsigned int i;

	if (!wiphy->bands[IEEE80211_BAND_5GHZ])
		return;

	sband = wiphy->bands[IEEE80211_BAND_5GHZ];

	for (i = 0; i < sband->n_channels; i++) {
		ch = &sband->channels[i];
		if (!ath9k_is_radar_freq(ch->center_freq))
			continue;
		/* We always enable radar detection/DFS on this
		 * frequency range. Additionally we also apply on
		 * this frequency range:
		 * - If STA mode does not yet have DFS supports disable
		 *   active scanning
		 * - If adhoc mode does not support DFS yet then
		 *   disable adhoc in the frequency.
		 * - If AP mode does not yet support radar detection/DFS
		 *   do not allow AP mode
		 */
		if (!(ch->flags & IEEE80211_CHAN_DISABLED))
			ch->flags |= IEEE80211_CHAN_RADAR |
				     IEEE80211_CHAN_NO_IBSS |
				     IEEE80211_CHAN_PASSIVE_SCAN;
	}
}

void ath9k_reg_apply_world_flags(struct wiphy *wiphy, enum reg_set_by setby)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath_softc *sc = hw->priv;
	struct ath_hal *ah = sc->sc_ah;

	switch (ah->regpair->regDmnEnum) {
	case 0x60:
	case 0x63:
	case 0x66:
	case 0x67:
		ath9k_reg_apply_5ghz_beaconing_flags(wiphy, setby);
		break;
	case 0x68:
		ath9k_reg_apply_5ghz_beaconing_flags(wiphy, setby);
		ath9k_reg_apply_active_scan_flags(wiphy, setby);
		break;
	}
	return;
}

int ath9k_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath_softc *sc = hw->priv;

	/* We always apply this */
	ath9k_reg_apply_radar_flags(wiphy);

	switch (request->initiator) {
	case REGDOM_SET_BY_DRIVER:
	case REGDOM_SET_BY_INIT:
	case REGDOM_SET_BY_CORE:
	case REGDOM_SET_BY_USER:
		break;
	case REGDOM_SET_BY_COUNTRY_IE:
		if (ath9k_is_world_regd(sc->sc_ah))
			ath9k_reg_apply_world_flags(wiphy, request->initiator);
		break;
	}

	return 0;
}

bool ath9k_regd_is_eeprom_valid(struct ath_hal *ah)
{
	u16 rd = ath9k_regd_get_eepromRD(ah);
	int i;

	if (rd & COUNTRY_ERD_FLAG) {
		/* EEPROM value is a country code */
		u16 cc = rd & ~COUNTRY_ERD_FLAG;
		for (i = 0; i < ARRAY_SIZE(allCountries); i++)
			if (allCountries[i].countryCode == cc)
				return true;
	} else {
		/* EEPROM value is a regpair value */
		for (i = 0; i < ARRAY_SIZE(regDomainPairs); i++)
			if (regDomainPairs[i].regDmnEnum == rd)
				return true;
	}
	DPRINTF(ah->ah_sc, ATH_DBG_REGULATORY,
		 "invalid regulatory domain/country code 0x%x\n", rd);
	return false;
}

/* EEPROM country code to regpair mapping */
static struct country_code_to_enum_rd*
ath9k_regd_find_country(u16 countryCode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(allCountries); i++) {
		if (allCountries[i].countryCode == countryCode)
			return &allCountries[i];
	}
	return NULL;
}

/* EEPROM rd code to regpair mapping */
static struct country_code_to_enum_rd*
ath9k_regd_find_country_by_rd(int regdmn)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(allCountries); i++) {
		if (allCountries[i].regDmnEnum == regdmn)
			return &allCountries[i];
	}
	return NULL;
}

/* Returns the map of the EEPROM set RD to a country code */
static u16 ath9k_regd_get_default_country(struct ath_hal *ah)
{
	u16 rd;

	rd = ath9k_regd_get_eepromRD(ah);
	if (rd & COUNTRY_ERD_FLAG) {
		struct country_code_to_enum_rd *country = NULL;
		u16 cc = rd & ~COUNTRY_ERD_FLAG;

		country = ath9k_regd_find_country(cc);
		if (country != NULL)
			return cc;
	}

	return CTRY_DEFAULT;
}

static struct reg_dmn_pair_mapping*
ath9k_get_regpair(int regdmn)
{
	int i;

	if (regdmn == NO_ENUMRD)
		return NULL;
	for (i = 0; i < ARRAY_SIZE(regDomainPairs); i++) {
		if (regDomainPairs[i].regDmnEnum == regdmn)
			return &regDomainPairs[i];
	}
	return NULL;
}

int ath9k_regd_init(struct ath_hal *ah)
{
	struct country_code_to_enum_rd *country = NULL;
	int regdmn;

	if (!ath9k_regd_is_eeprom_valid(ah)) {
		DPRINTF(ah->ah_sc, ATH_DBG_REGULATORY,
			"Invalid EEPROM contents\n");
		return -EINVAL;
	}

	ah->ah_countryCode = ath9k_regd_get_default_country(ah);

	if (ah->ah_countryCode == CTRY_DEFAULT &&
	    ath9k_regd_get_eepromRD(ah) == CTRY_DEFAULT)
		ah->ah_countryCode = CTRY_UNITED_STATES;

	if (ah->ah_countryCode == CTRY_DEFAULT) {
		regdmn = ath9k_regd_get_eepromRD(ah);
		country = NULL;
	} else {
		country = ath9k_regd_find_country(ah->ah_countryCode);
		if (country == NULL) {
			DPRINTF(ah->ah_sc, ATH_DBG_REGULATORY,
				"Country is NULL!!!!, cc= %d\n",
				ah->ah_countryCode);
			return -EINVAL;
		} else
			regdmn = country->regDmnEnum;
	}

	ah->ah_currentRDInUse = regdmn;
	ah->regpair = ath9k_get_regpair(regdmn);

	if (!ah->regpair) {
		DPRINTF(ah->ah_sc, ATH_DBG_FATAL,
			"No regulatory domain pair found, cannot continue\n");
		return -EINVAL;
	}

	if (!country)
		country = ath9k_regd_find_country_by_rd(regdmn);

	if (country) {
		ah->alpha2[0] = country->isoName[0];
		ah->alpha2[1] = country->isoName[1];
	} else {
		ah->alpha2[0] = '0';
		ah->alpha2[1] = '0';
	}

	DPRINTF(ah->ah_sc, ATH_DBG_REGULATORY,
		"Country alpha2 being used: %c%c\n"
		"Regpair detected: 0x%0x\n",
		ah->alpha2[0], ah->alpha2[1],
		ah->regpair->regDmnEnum);

	return 0;
}

u32 ath9k_regd_get_ctl(struct ath_hal *ah, struct ath9k_channel *chan)
{
	u32 ctl = NO_CTL;

	if (!ah->regpair ||
	    (ah->ah_countryCode == CTRY_DEFAULT && isWwrSKU(ah))) {
		if (IS_CHAN_B(chan))
			ctl = SD_NO_CTL | CTL_11B;
		else if (IS_CHAN_G(chan))
			ctl = SD_NO_CTL | CTL_11G;
		else
			ctl = SD_NO_CTL | CTL_11A;
		return ctl;
	}

	if (IS_CHAN_B(chan))
		ctl = ah->regpair->reg_2ghz_ctl | CTL_11B;
	else if (IS_CHAN_G(chan))
		ctl = ah->regpair->reg_5ghz_ctl | CTL_11G;
	else
		ctl = ah->regpair->reg_5ghz_ctl | CTL_11A;

	return ctl;
}
