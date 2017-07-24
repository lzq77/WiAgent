#ifndef BEACON_H
#define BEACON_H

#include "hostapd.h"

u16 hostapd_own_capab_info(struct hostapd_data *hapd, struct sta_info *sta,
			   int probe);
u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid);
const u8 * wpa_auth_get_wpa_ie(struct wpa_authenticator *wpa_auth, size_t *len);
u8 * hostapd_eid_ht_capabilities(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ht_operation(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ext_capab(struct hostapd_data *hapd, u8 *eid);
int ieee802_11_build_ap_params(struct hostapd_data *hapd,u8 *da,u8 *bssid,
					const char *ssid,int ssid_len,int probe,struct wpa_driver_ap_params *params);
					
u8 * hostapd_eid_bss_max_idle_period(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_qos_map_set(struct hostapd_data *hapd, u8 *eid);					
u8 * generate_assoc_resp(struct hostapd_data *hapd, struct sta_info *sta, u8 *vbssid,
			    u16 status_code, int reassoc,int *frame_len);
				   
#endif
