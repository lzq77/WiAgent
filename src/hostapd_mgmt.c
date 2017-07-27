
#include "hostapd.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include "nl80211_copy.h"
#include "beacon.h"
#include "wi_bssid_mask.h"
#include "hostapd_mgmt.h"

int wi_handle_beacon(struct hostapd_data *_hapd,u8 *da,u8 *bssid,
					const char *ssid,int ssid_len)
{
	u8 *packet;
    struct hostapd_data *hapd = _hapd;
	struct wpa_driver_ap_params params;
	int packet_len = 0;

	ieee802_11_build_ap_params(hapd,da,bssid,ssid,ssid_len, 0,&params);//产生beacon帧,头部和EX字段分开
	
    packet_len = params.head_len + params.tail_len;
	packet = (u8 *)os_zalloc(packet_len);//为beacon分配内存
	if (!packet)
		goto free_params;

	os_memcpy(packet, params.head, params.head_len);//复制beacon的头部
	os_memcpy(packet+params.head_len, params.tail, params.tail_len);//复制beacon的尾部

    if (hostapd_drv_send_mlme(hapd, packet, packet_len, 1) < 0) {
		wpa_printf(MSG_INFO, "handle_beacon: send failed\n");
        return -1;
    }
    
	os_free(params.head);
	os_free(params.tail);
	os_free(packet);

	return 0;

free_params:
	os_free(params.head);
	os_free(params.tail);
	os_free(packet);
	return -1;
}

int wi_handle_probe_req(struct hostapd_data *_hapd,u8 *da,u8 *bssid,
					const char *ssid,int ssid_len)
{

	wpa_printf(MSG_INFO, "wi_handle_probe_req,da:"MACSTR", bssid:"MACSTR",\n",
            MAC2STR(da), MAC2STR(bssid));
	
    u8 *packet;
    struct hostapd_data *hapd = _hapd;
	struct wpa_driver_ap_params params;
	int beacon_len = 0;

    u8 hw_addr[6] = {0xc4, 0x04, 0x15, 0x9c, 0xf6, 0xe3};

    char *debugfs_file = "/sys/kernel/debug/ieee80211/phy0/ath9k/bssid_extra";
    set_bssid_mask(debugfs_file, hw_addr, bssid);
    
    /**
     * Generating probe response frame.
     */
	ieee802_11_build_ap_params(hapd,da,bssid,ssid,ssid_len, true, &params);
	
    beacon_len = params.head_len + params.tail_len;
	packet = (u8 *)os_zalloc(beacon_len);//为beacon分配内存
	if (!packet)
		goto free_params;

	os_memcpy(packet, params.head, params.head_len);//复制beacon的头部
	os_memcpy(packet+params.head_len, params.tail, params.tail_len);//复制beacon的尾部

    if (hostapd_drv_send_mlme(hapd, packet, beacon_len, 1) < 0) {
		wpa_printf(MSG_INFO, "handle_beacon: send failed\n");
        return -1;
    }
    
	os_free(params.head);
	os_free(params.tail);
	os_free(packet);

	return 0;

free_params:
	os_free(params.head);
	os_free(params.tail);
	os_free(packet);
	
    return -1;
}

static void
send_auth_reply(struct hostapd_data *hapd,
			    const u8 *dst, const u8 *bssid,
			    u16 auth_alg, u16 auth_transaction, u16 resp)
{
	struct ieee80211_mgmt *reply;
	u8 *buf;
	size_t rlen;

	rlen = IEEE80211_HDRLEN + sizeof(reply->u.auth);
	buf = os_zalloc(rlen);
	if (buf == NULL)
		return;

	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					    WLAN_FC_STYPE_AUTH);
	os_memcpy(reply->da, dst, ETH_ALEN);
	os_memcpy(reply->sa, bssid, ETH_ALEN);
	os_memcpy(reply->bssid, bssid, ETH_ALEN);

	reply->u.auth.auth_alg = host_to_le16(auth_alg);
	reply->u.auth.auth_transaction = host_to_le16(auth_transaction);
	reply->u.auth.status_code = host_to_le16(resp);

	if (hostapd_drv_send_mlme(hapd, (u8 *)reply, rlen, 0) < 0)
		wpa_printf(MSG_INFO, "send_auth_reply: send");

	os_free(buf);
}

static void wi_handle_auth(struct hostapd_data *hapd,
			const struct ieee80211_mgmt *mgmt)
{
	wpa_printf(MSG_INFO, "wi_handle_auth.\n");
	u16 auth_alg, auth_transaction, status_code;
	u16 resp = WLAN_STATUS_SUCCESS;
	struct sta_info *sta = NULL;

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);

	sta = ap_sta_add(hapd, mgmt->sa);
	if (!sta) {
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		wpa_printf(MSG_INFO, "ap_add_sta failed.\n");
		return;
	}

	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
		sta->flags |= WLAN_STA_AUTH;
		sta->auth_alg = WLAN_AUTH_OPEN;
		break;
	case WLAN_AUTH_SHARED_KEY:
		break;
	}

	send_auth_reply(hapd, mgmt->sa, mgmt->bssid, auth_alg,
			auth_transaction + 1, resp);
}

/***
 * proberesponse construct
 */
static int send_assoc_resp(struct hostapd_data *hapd, struct sta_info *sta, u16 status_code, int reassoc, u8 *vbssid)
{
    int send_len;
	u8 buf[sizeof(struct ieee80211_mgmt) + 1024];
	struct ieee80211_mgmt *reply;
	u8 *p;

	os_memset(buf, 0, sizeof(buf));
	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT,
			     (reassoc ? WLAN_FC_STYPE_REASSOC_RESP :
			      WLAN_FC_STYPE_ASSOC_RESP));

	os_memcpy(reply->da, sta->addr, ETH_ALEN);
	os_memcpy(reply->sa, vbssid, ETH_ALEN);
	os_memcpy(reply->bssid, vbssid, ETH_ALEN);//virtual bssid --nm
	
	send_len = IEEE80211_HDRLEN;
	send_len += sizeof(reply->u.assoc_resp);	
	reply->u.assoc_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd, sta, 0));
		
	reply->u.assoc_resp.status_code = host_to_le16(status_code);
	reply->u.assoc_resp.aid = host_to_le16(sta->aid | BIT(14) | BIT(15));
	
	/* Supported rates */
	p = hostapd_eid_supp_rates(hapd, reply->u.assoc_resp.variable);

	/* Extended supported rates */
	p = hostapd_eid_ext_supp_rates(hapd, p);

	p = hostapd_eid_ht_capabilities(hapd, p);
	p = hostapd_eid_ht_operation(hapd, p);

	p = hostapd_eid_ext_capab(hapd, p);
	//p = hostapd_eid_bss_max_idle_period(hapd, p);
	if (sta->qos_map_enabled)
		p = hostapd_eid_qos_map_set(hapd, p);
	//TODO:SDWN --nm 
	sta->flags |= WLAN_STA_WMM;
	if (sta->flags & WLAN_STA_WMM)
		p = hostapd_eid_wmm(hapd, p);
#ifdef CONFIG_WPS
	if ((sta->flags & WLAN_STA_WPS) ||
	    ((sta->flags & WLAN_STA_MAYBE_WPS) && hapd->conf->wpa)) {
		struct wpabuf *wps = wps_build_assoc_resp_ie();
		if (wps) {
			os_memcpy(p, wpabuf_head(wps), wpabuf_len(wps));
			p += wpabuf_len(wps);
			wpabuf_free(wps);
		}
	}
#endif /* CONFIG_WPS */

	send_len += p - reply->u.assoc_resp.variable;

	return hostapd_drv_send_mlme(hapd, reply, send_len, 0);
}

static void wi_handle_assoc(struct hostapd_data *hapd,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 int reassoc, u8 *vbssid)
{
	wpa_printf(MSG_INFO, "wi_handle_assoc. len = %d\n", len);
	u16 capab_info, listen_interval;
	u16 resp = WLAN_STATUS_SUCCESS;
	const u8 *pos;
	int left, i;
	struct sta_info *sta;

	//头部检验:帧的总长度不小于头部的长度
	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req)) {
		wpa_printf(MSG_INFO, "handle_assoc(reassoc=%d) - too short payload (len=%lu)",
			   reassoc, (unsigned long) len);
		return;
	}
	if (reassoc) {
		capab_info = le_to_host16(mgmt->u.reassoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.reassoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "reassociation request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d current_ap="
			   MACSTR,
			   MAC2STR(mgmt->sa), capab_info, listen_interval,
			   MAC2STR(mgmt->u.reassoc_req.current_ap));
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req));
		pos = mgmt->u.reassoc_req.variable;
	} else {
		capab_info = le_to_host16(mgmt->u.assoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.assoc_req.listen_interval);
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req));
		pos = mgmt->u.assoc_req.variable;
	}

	sta = ap_get_sta(hapd, mgmt->sa);
	if (sta == NULL || (sta->flags & WLAN_STA_AUTH) == 0) {
		//send_deauth(hapd, mgmt->sa,
		//	    WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA);
        wpa_printf(MSG_DEBUG, "staion is null association has not authriened\n");
		return;
	}

	if (listen_interval > hapd->conf->max_listen_interval) {
		wpa_printf(MSG_DEBUG, "Too large Listen Interval (%d)",
			       listen_interval);
		resp = WLAN_STATUS_ASSOC_DENIED_LISTEN_INT_TOO_LARGE;
		goto fail;
	}

	/* followed by SSID and Supported rates; and HT capabilities if 802.11n
	 * is used */
	resp = check_assoc_ies(hapd, sta, pos, left, reassoc);
	if (resp != WLAN_STATUS_SUCCESS) {
        wpa_printf(MSG_DEBUG,"resp != WLAN_STATUS_SUCCESS\n");
		goto fail;
    }

	if (hostapd_get_aid(hapd, sta) < 0) {
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
        wpa_printf(MSG_DEBUG,"hostapd_get_aid failed\n");
		goto fail;
	}

	sta->capability = capab_info;
	sta->listen_interval = listen_interval;

	if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		sta->flags |= WLAN_STA_NONERP;
	for (i = 0; i < sta->supported_rates_len; i++) {
		if ((sta->supported_rates[i] & 0x7f) > 22) {
			sta->flags &= ~WLAN_STA_NONERP;
			break;
		}
	}

    /* 重新设置beacon帧
	if (sta->flags & WLAN_STA_NONERP && !sta->nonerp_set) {
		sta->nonerp_set = 1;
		hapd->iface->num_sta_non_erp++;
		if (hapd->iface->num_sta_non_erp == 1)
			ieee802_11_set_beacons(hapd->iface);
	}
    */

    /*如果不使用短间隔，则重新设置beacon帧
	if (!(sta->capability & WLAN_CAPABILITY_SHORT_SLOT_TIME) &&
	    !sta->no_short_slot_time_set) {
		sta->no_short_slot_time_set = 1;
		hapd->iface->num_sta_no_short_slot_time++;
		if (hapd->iface->current_mode->mode ==
		    HOSTAPD_MODE_IEEE80211G &&
		    hapd->iface->num_sta_no_short_slot_time == 1)
			ieee802_11_set_beacons(hapd->iface);
	}
    */

	if (sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE)
		sta->flags |= WLAN_STA_SHORT_PREAMBLE;
	else
		sta->flags &= ~WLAN_STA_SHORT_PREAMBLE;

    /* 如果不支持短前导码，则重新设置beacon帧
	if (!(sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE) &&
	    !sta->no_short_preamble_set) {
		sta->no_short_preamble_set = 1;
		hapd->iface->num_sta_no_short_preamble++;
		if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_preamble == 1)
			ieee802_11_set_beacons(hapd->iface);
	}
    */

	/* Station will be marked associated, after it acknowledges AssocResp
	 */
	sta->flags |= WLAN_STA_ASSOC_REQ_OK;
    sta->flags |= WLAN_STA_AUTHORIZED;  //kernel will use --nm

	/* Make sure that the previously registered inactivity timer will not
	 * remove the STA immediately. */
	sta->timeout_next = STA_NULLFUNC;

	if (send_assoc_resp(hapd, sta, resp, reassoc, vbssid) < 0) {
	   wpa_printf(MSG_INFO, "Failed to send assoc resp: %s",
			   strerror(errno)); 
       return;
    }

    hostapd_handle_assoc_cb(hapd, mgmt->sa);//通知内核添加sta_info

    return;

fail:
    wpa_printf(MSG_DEBUG,"handle_assoc failed\n");
	return;
}

static void 
wi_mlme_event_mgmt(struct i802_bss *bss, 
                struct nlattr *freq, struct nlattr *sig,
			    const u8 *frame, size_t len)
{
    fprintf(stderr, "wi_mlme_event_mgmt\n");

   struct wpa_driver_nl80211_data *drv = bss->drv;
   struct hostapd_data *hapd = (struct hostapd_data *)drv->ctx;
   struct ieee80211_mgmt *mgmt;
   u16 fc, stype;
   int ssi_signal = 0;
   int rx_freq = 0;
   
    char bssid[6] = {0x00, 0x1b, 0xb3, 0x8b, 0x88, 0xa3};
    char *ssid = "wimaster";
    int ssid_len = strlen(ssid);


   mgmt = (const struct ieee80211_mgmt *) frame;
   if (len < 24) {
       wpa_printf(MSG_DEBUG, "nl80211: Too short management frame");
       return;
   }
   
   fc = le_to_host16(mgmt->frame_control);
   stype = WLAN_FC_GET_STYPE(fc);

   if (sig)
       ssi_signal = (s32) nla_get_u32(sig);

   if (freq) 
		rx_freq = drv->last_mgmt_freq;



   wpa_printf(MSG_DEBUG, "Management frame stype(%d), bssid("MACSTR")\n", 
                    stype, MAC2STR(mgmt->bssid));

   switch (stype) {
        case WLAN_FC_STYPE_PROBE_REQ:
            wi_handle_probe_req(hapd, mgmt->sa, bssid,
                    ssid, ssid_len);
            break;
        case WLAN_FC_STYPE_AUTH:
            wi_handle_auth(hapd, mgmt);
            break;
        case WLAN_FC_STYPE_DEAUTH:
            break;
        case WLAN_FC_STYPE_ASSOC_REQ:
            wi_handle_assoc(hapd, mgmt, len, false, mgmt->bssid);
            break;
        case WLAN_FC_STYPE_DISASSOC:
            break;
        case WLAN_FC_STYPE_REASSOC_RESP:
            break;
        case WLAN_FC_STYPE_ACTION:
            break;
   }
}

static void 
wi_mlme_event(struct i802_bss *bss, enum nl80211_commands cmd, 
        struct nlattr *frame, struct nlattr *addr, 
        struct nlattr *timed_out, struct nlattr *freq, 
        struct nlattr *ack, struct nlattr *cookie, struct nlattr *sig)
{
    fprintf(stderr, "wi_mlme_event\n");
	struct wpa_driver_nl80211_data *drv = bss->drv;
	const u8 *data;
    size_t len;
	
	data = (const u8 *)nla_data(frame);
	len = nla_len(frame);
	if (len < 4 + 2 * ETH_ALEN) {
		wpa_printf(MSG_MSGDUMP, "nl80211: MLME event %d (%s) on %s("
			   MACSTR ") - too short",
			   cmd, nl80211_command_to_string(cmd), bss->ifname,
			   MAC2STR(bss->addr));
		return;
	}
	
	switch (cmd) {
	case NL80211_CMD_FRAME:
		wi_mlme_event_mgmt(bss, freq, sig, (const u8*)nla_data(frame),
			nla_len(frame));
	break;
	
	default:
		break;
	}	
}

int wi_process_bss_event(struct nl_msg *msg, void *arg)
{
    fprintf(stderr, "wi_process_bss_event: received frame\n");
    struct i802_bss *bss = (struct i802_bss *)arg;
	struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
		
	switch (gnlh->cmd) {
	case NL80211_CMD_FRAME:
	case NL80211_CMD_FRAME_TX_STATUS:
	//need to notice gnlh->cmd is u8 type --nm
		wi_mlme_event(bss, (enum nl80211_commands)gnlh->cmd, tb[NL80211_ATTR_FRAME],
			   tb[NL80211_ATTR_MAC], tb[NL80211_ATTR_TIMED_OUT],
			   tb[NL80211_ATTR_WIPHY_FREQ], tb[NL80211_ATTR_ACK],
			   tb[NL80211_ATTR_COOKIE],
			   tb[NL80211_ATTR_RX_SIGNAL_DBM]);
		break;
	case NL80211_CMD_UNEXPECTED_FRAME:
		//nl80211_spurious_frame(bss, tb, 0);
		break;
	case NL80211_CMD_UNEXPECTED_4ADDR_FRAME:
		//nl80211_spurious_frame(bss, tb, 1);
		break;
	default:
		wpa_printf(MSG_DEBUG, "nl80211: Ignored unknown event "
			   "(cmd=%d)", gnlh->cmd);
		break;
	}
	return NL_SKIP;	
}


