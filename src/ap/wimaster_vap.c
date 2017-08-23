#include "ieee802_1x_defs.h"
#include "wimaster_vap.h"

static struct vap_data *vap_first = NULL;
static struct vap_data *vap_last = NULL;

static u8 bssid_mask[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static char *bssid_file_path = "/sys/kernel/debug/ieee80211/phy0/ath9k/bssid_extra";
static u8 hw_addr[6] = {0xc4, 0x04, 0x15, 0x9c, 0xf6, 0xe3};

static void compute_bssid_mask(const u8 *hw, const u8 *bssid)
{
    // For each VAP, update the bssid mask to include
    // the common bits of all VAPs.
    int i = 0;
    for (; i < 6; i++)
    {
          bssid_mask[i] &= ~(hw[i] ^ bssid[i]);
    }
}

/*
 * This re-computes the BSSID mask for this node
 * using all the BSSIDs of the VAPs, and sets the
 * hardware register accordingly.
 */
static void reset_bssid_mask(void)
{
    struct vap_data *vap_temp = vap_first;
    while (vap_temp !=NULL) {
        compute_bssid_mask(hw_addr, vap_temp->bssid);
        vap_temp = vap_temp->next;
    }
    // Update bssid mask register through debugfs
    FILE *debugfs_file = fopen(bssid_file_path, "w");

    if (debugfs_file!=NULL) {
        fprintf(debugfs_file,MACSTR"\n", MAC2STR(bssid_mask));
        fclose(debugfs_file);
    }
}


struct vap_data *wimaster_get_vap(const u8 *addr)
{
    struct vap_data *vap_temp = vap_first;

    while (vap_temp !=  NULL) {
        if (os_memcmp(vap_temp->addr, addr, ETH_ALEN) == 0) 
            return vap_temp;
        vap_temp = vap_temp->next;
    }
    return NULL;
}

static void wimaster_vap_list()
{
    int num;
    struct vap_data *vap_temp = vap_first;

    wpa_printf(MSG_INFO, "\nwi vap list:\n");
    while (vap_temp) {
        wpa_printf(MSG_INFO, "%d. vap:"MACSTR" bssid:"
                MACSTR" ssid:%s\n", ++num, MAC2STR(vap_temp->addr),
                MAC2STR(vap_temp->bssid), vap_temp->ssid);
        vap_temp =  vap_temp->next;
    }

}

struct vap_data * wimaster_vap_add(const u8 *addr, const u8 *bssid, const char *ssid)
{
    struct vap_data *vap_temp;

    vap_temp = wimaster_get_vap(addr);
    if (vap_temp) 
        return vap_temp;

    vap_temp = (struct vap_data *)os_zalloc(sizeof(struct vap_data));
    if(vap_temp == NULL) {
        fprintf(stderr, "vap malloc failed, memory is not enough!\n");
        return NULL;
    }

    if (vap_first == NULL) {
        vap_first = vap_temp;
        vap_last = vap_first;
    }
    else {
        vap_last->next = vap_temp;
        vap_last = vap_temp;
    } 
    os_memcpy(vap_last->addr, addr, ETH_ALEN);
    os_memcpy(vap_last->bssid, bssid, ETH_ALEN);
    vap_last->ssid = (char *)os_zalloc(strlen(ssid) + 1);
    strcpy(vap_last->ssid, ssid);
    vap_last->next = NULL;

    reset_bssid_mask();

    return vap_last;
}

int wimaster_vap_remove(const u8 *addr)
{
    struct vap_data *vap_temp = vap_first;
    struct vap_data *vap_previous = NULL;

    while (vap_temp !=  NULL) {
        if (os_memcmp(vap_temp->addr, addr, ETH_ALEN) == 0) {

            if(vap_previous) {
                vap_previous->next = vap_temp->next;
            }
            else {
                vap_first = vap_temp->next;
            }
            
            if (vap_temp->ssid) {
                os_free(vap_temp->ssid);
                vap_temp->ssid = NULL;
            }
            os_free(vap_temp);
            vap_temp = NULL;
            
            break;
        }
        
        vap_previous = vap_temp;
        vap_temp = vap_temp->next;

    }
    reset_bssid_mask();
    return 0;
}

int wimaster_for_each_vap(int (*cb)(struct vap_data *vap, void *ctx), void *ctx)
{
    struct vap_data *vap;
    for(vap = vap_first; vap; vap = vap->next) {
        if(cb(vap, ctx))
            return 1;
    }
    return 0;
}

static const char *stainfo_to_json(const struct sta_info *sta, 
        const struct ieee80211_ht_capabilities *ht_cap)
{
    const char *stainfo_json;
    int rate_len;
    int i;
	struct json_object *json = json_object_new_object();//main object
	struct json_object *json_supported_rates = json_object_new_array();//supported_rates
	struct json_object *json_ht_cap = json_object_new_object();//ht_cap
	struct json_object *supported_mcs_set = json_object_new_array();//ht_cap.supported_mcs_set

	//construct json
    json_object_object_add(json, "aid", json_object_new_int(sta->aid));
    json_object_object_add(json, "cap", json_object_new_int(sta->capability));
    json_object_object_add(json, "interval", json_object_new_int(sta->listen_interval));
    json_object_object_add(json, "flags", json_object_new_int(sta->flags));
    json_object_object_add(json, "qosinfo", json_object_new_int(sta->qosinfo));
    json_object_object_add(json, "vht_opmode", json_object_new_int(sta->vht_opmode));
    json_object_object_add(json, "supported_rates_len", json_object_new_int(sta->supported_rates_len));

	//construst json_supported_rates
	rate_len = sta->supported_rates_len;
	for(i = 0; i < rate_len; i++){
		json_object_array_add(json_supported_rates,json_object_new_int(sta->supported_rates[i]));
	}
	//add  json_supported_rates to json
	json_object_object_add(json, "supported_rates", json_supported_rates);

	//construst json_ht_cap
	json_object_object_add(json_ht_cap, "ht_capabilities_info", json_object_new_int(ht_cap->ht_capabilities_info));
	json_object_object_add(json_ht_cap, "a_mpdu_params", json_object_new_int(ht_cap->a_mpdu_params));
	json_object_object_add(json_ht_cap, "ht_extended_capabilities", json_object_new_int(ht_cap->ht_extended_capabilities));
	json_object_object_add(json_ht_cap, "tx_bf_capability_info", json_object_new_int(ht_cap->tx_bf_capability_info));
	json_object_object_add(json_ht_cap,"asel_capabilities",json_object_new_int(ht_cap->asel_capabilities));

	//construst supported_mcs_set
	for(i = 0; i < 15; i++){
		json_object_array_add(supported_mcs_set,json_object_new_int(ht_cap->supported_mcs_set[i]));
	}
	//add  supported_mcs_set to ht_cap
	json_object_object_add(json_ht_cap,"supported_mcs_set",supported_mcs_set);

	//add  json_ht_cap to json
	json_object_object_add(json, "ht_cap", json_ht_cap);

    stainfo_json = json_object_to_json_string(json);

    json_object_put(json);

    return stainfo_json;
}

const char *get_stainfo_json(struct hostapd_data *hapd, const u8 *addr)
{
    struct sta_info *sta;
    struct ieee80211_ht_capabilities ht_cap;

    sta = ap_get_sta(hapd, addr);
    if(!sta) {
        wpa_printf(MSG_ERROR, "sta_info do not exist\n");
        return NULL;
    }

    if(sta->flags & WLAN_STA_HT) {
        wpa_printf(MSG_DEBUG, "station("MACSTR") support HT\n",
                MAC2STR(addr));
        hostapd_get_ht_capab(hapd, sta->ht_capabilities, &ht_cap);
    }

    return stainfo_to_json(sta, &ht_cap);
}

