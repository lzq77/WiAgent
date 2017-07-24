
#include "com.h"
#include "hw_features.h"
#include "driver_nl80211.h"

char * dfs_info(struct hostapd_channel_data *chan)
{
	static char info[256];
	const char *state;

	switch (chan->flag & HOSTAPD_CHAN_DFS_MASK) {
	case HOSTAPD_CHAN_DFS_UNKNOWN:
		state = "unknown";
		break;
	case HOSTAPD_CHAN_DFS_USABLE:
		state = "usable";
		break;
	case HOSTAPD_CHAN_DFS_UNAVAILABLE:
		state = "unavailable";
		break;
	case HOSTAPD_CHAN_DFS_AVAILABLE:
		state = "available";
		break;
	default:
		return NULL;
	}
	os_snprintf(info, sizeof(info), " (DFS state = %s)", state);
	info[sizeof(info) - 1] = '\0';

	return info;
}

void hostapd_free_hw_features(struct hostapd_hw_modes *hw_features,
			      size_t num_hw_features)
{
	size_t i;

	if (hw_features == NULL)
		return;

	for (i = 0; i < num_hw_features; i++) {
		os_free(hw_features[i].channels);
		os_free(hw_features[i].rates);
	}

	os_free(hw_features);
}


int hostapd_hw_get_freq(struct hostapd_data *hapd, int chan)
{
	int i;

	if (!hapd->iface->current_mode)
		return 0;

	for (i = 0; i < hapd->iface->current_mode->num_channels; i++) {
		struct hostapd_channel_data *ch =
			&hapd->iface->current_mode->channels[i];
		if (ch->chan == chan)
			return ch->freq;
	}

	return 0;
}


//drv_ops
int hostapd_set_freq_params(struct hostapd_freq_params *data, int mode,
			    int freq, int channel, int ht_enabled,
			    int vht_enabled, int sec_channel_offset,
			    int vht_oper_chwidth, int center_segment0,
			    int center_segment1, u32 vht_caps)
{
	int tmp;

	os_memset(data, 0, sizeof(*data));
	data->mode = mode;
	data->freq = freq;
	data->channel = channel;
	data->ht_enabled = ht_enabled;
	data->vht_enabled = vht_enabled;
	data->sec_channel_offset = sec_channel_offset;
	data->center_freq1 = freq + sec_channel_offset * 10;
	data->center_freq2 = 0;
	data->bandwidth = sec_channel_offset ? 40 : 20;

	/*
	 * This validation code is probably misplaced, maybe it should be
	 * in src/ap/hw_features.c and check the hardware support as well.
	 */
	if (data->vht_enabled) switch (vht_oper_chwidth) {
	case VHT_CHANWIDTH_USE_HT:
		if (center_segment1)
			return -1;
		if (center_segment0 != 0 &&
		    5000 + center_segment0 * 5 != data->center_freq1 &&
		    2407 + center_segment0 * 5 != data->center_freq1)
			return -1;
		break;
	case VHT_CHANWIDTH_80P80MHZ:
		if (!(vht_caps & VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ)) {
			wpa_printf(MSG_ERROR,
				   "80+80 channel width is not supported!");
			return -1;
		}
		if (center_segment1 == center_segment0 + 4 ||
		    center_segment1 == center_segment0 - 4)
			return -1;
		data->center_freq2 = 5000 + center_segment1 * 5;
		/* fall through */
	case VHT_CHANWIDTH_80MHZ:
		data->bandwidth = 80;
		if (vht_oper_chwidth == 1 && center_segment1)
			return -1;
		if (vht_oper_chwidth == 3 && !center_segment1)
			return -1;
		if (!sec_channel_offset)
			return -1;
		/* primary 40 part must match the HT configuration */
		tmp = (30 + freq - 5000 - center_segment0 * 5)/20;
		tmp /= 2;
		if (data->center_freq1 != 5000 +
					 center_segment0 * 5 - 20 + 40 * tmp)
			return -1;
		data->center_freq1 = 5000 + center_segment0 * 5;
		break;
	case VHT_CHANWIDTH_160MHZ:
		data->bandwidth = 160;
		if (!(vht_caps & (VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
				  VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ))) {
			wpa_printf(MSG_ERROR,
				   "160MHZ channel width is not supported!");
			return -1;
		}
		if (center_segment1)
			return -1;
		if (!sec_channel_offset)
			return -1;
		/* primary 40 part must match the HT configuration */
		tmp = (70 + freq - 5000 - center_segment0 * 5)/20;
		tmp /= 2;
		if (data->center_freq1 != 5000 +
					 center_segment0 * 5 - 60 + 40 * tmp)
			return -1;
		data->center_freq1 = 5000 + center_segment0 * 5;
		break;
	}

	return 0;
}


//drv_ops.c
int hostapd_set_freq(struct hostapd_data *hapd, int mode, int freq,
		     int channel, int ht_enabled, int vht_enabled,
		     int sec_channel_offset, int vht_oper_chwidth,
		     int center_segment0, int center_segment1)
{
	struct hostapd_freq_params data;

	if (hostapd_set_freq_params(&data, mode, freq, channel, ht_enabled,
				    vht_enabled, sec_channel_offset,
				    vht_oper_chwidth,
				    center_segment0, center_segment1,
				    hapd->iface->current_mode->vht_capab))
		return -1;

	if (hapd->driver == NULL)
		return 0;
	if (hapd->driver->set_freq == NULL)
		return 0;
	return hapd->driver->set_freq(hapd->drv_priv, &data);
}

int hostapd_rate_found(int *list, int rate)
{
	int i;

	if (list == NULL)
		return 0;

	for (i = 0; list[i] >= 0; i++)
		if (list[i] == rate)
			return 1;

	return 0;
}

int hostapd_prepare_rates(struct hostapd_iface *iface,
			  struct hostapd_hw_modes *mode)
{
	int i, num_basic_rates = 0;
	int basic_rates_a[] = { 60, 120, 240, -1 };
	int basic_rates_b[] = { 10, 20, -1 };
	int basic_rates_g[] = { 10, 20, 55, 110, -1 };
	int *basic_rates;
	//wpa_printf(MSG_DEBUG, "hostapd prepare set rates num is %d\n",mode->num_rates);
	if (iface->conf->basic_rates)
		basic_rates = iface->conf->basic_rates;
	else switch (mode->mode) {
	case HOSTAPD_MODE_IEEE80211A:
		basic_rates = basic_rates_a;
		break;
	case HOSTAPD_MODE_IEEE80211B:
		basic_rates = basic_rates_b;
		break;
	case HOSTAPD_MODE_IEEE80211G:
		basic_rates = basic_rates_g;
		break;
	case HOSTAPD_MODE_IEEE80211AD:
		return 0; /* No basic rates for 11ad */
	default:
		return -1;
	}

	i = 0;
	while (basic_rates[i] >= 0)
		i++;
	if (i)
		i++; /* -1 termination */
	os_free(iface->basic_rates);
	iface->basic_rates = (int *)os_malloc(i * sizeof(int));
	if (iface->basic_rates)
		os_memcpy(iface->basic_rates, basic_rates, i * sizeof(int));

	os_free(iface->current_rates);
	iface->num_rates = 0;

	iface->current_rates =
		(struct hostapd_rate_data*)os_calloc(mode->num_rates, sizeof(struct hostapd_rate_data));
	if (!iface->current_rates) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for rate "
			   "table.");
		return -1;
	}

	for (i = 0; i < mode->num_rates; i++) {
		struct hostapd_rate_data *rate;

		if (iface->conf->supported_rates &&
		    !hostapd_rate_found(iface->conf->supported_rates,
					mode->rates[i]))
			continue;

		rate = &iface->current_rates[iface->num_rates];
		rate->rate = mode->rates[i];
		if (hostapd_rate_found(basic_rates, rate->rate)) {
			rate->flags |= HOSTAPD_RATE_BASIC;
			num_basic_rates++;
		}
		//DEBUG
		//wpa_printf(MSG_DEBUG, "RATE[%d] rate=%d flags=0x%x",
		//	   iface->num_rates, rate->rate, rate->flags);
		iface->num_rates++;
	}

	if ((iface->num_rates == 0 || num_basic_rates == 0) &&
	    (!iface->conf->ieee80211n || !iface->conf->require_ht)) {
		wpa_printf(MSG_ERROR, "No rates remaining in supported/basic "
			   "rate sets (%d,%d).",
			   iface->num_rates, num_basic_rates);
		return -1;
	}

	return 0;
}

const char * hostapd_hw_mode_txt(int mode)
{
	switch (mode) {
	case HOSTAPD_MODE_IEEE80211A:
		return "IEEE 802.11a";
	case HOSTAPD_MODE_IEEE80211B:
		return "IEEE 802.11b";
	case HOSTAPD_MODE_IEEE80211G:
		return "IEEE 802.11g";
	case HOSTAPD_MODE_IEEE80211AD:
		return "IEEE 802.11ad";
	default:
		return "UNKNOWN";
	}
}

