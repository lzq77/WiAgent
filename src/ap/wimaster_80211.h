#ifndef WIMASTER_80211_H
#define WIMASTER_80211_H

#include "../utils/common.h"
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <event2/util.h>

void hostapd_wpa_event(void *ctx, enum wpa_event_type event,
		       union wpa_event_data *data);

int wimaster_handle_beacon(struct hostapd_data *_hapd, u8 *da, u8 *bssid,
					const char *ssid, int ssid_len);

int wimaster_handle_probe_req(struct hostapd_data *_hapd,u8 *da,u8 *bssid,
					const char *ssid,int ssid_len);

int wimaster_process_bss_event(struct nl_msg *msg, void *arg);

void wimaster_send_beacon(evutil_socket_t fd, short what, void *arg);

#endif
