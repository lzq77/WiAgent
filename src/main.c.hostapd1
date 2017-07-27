#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include "nl80211_copy.h"
#include "beacon.h"

void send_deauth(struct hostapd_data *hapd, const u8 *addr,
			u16 reason_code)
{
	int send_len;
	struct ieee80211_mgmt reply;
    char bssid[6] = {0x00, 0x1b, 0xb3, 0x3c, 0x5a, 0x11};


	os_memset(&reply, 0, sizeof(reply));
	reply.frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_DEAUTH);
	os_memcpy(reply.da, addr, ETH_ALEN);
	os_memcpy(reply.sa, bssid, ETH_ALEN);
	os_memcpy(reply.bssid, bssid, ETH_ALEN);

	send_len = IEEE80211_HDRLEN + sizeof(reply.u.deauth);
	reply.u.deauth.reason_code = host_to_le16(reason_code);

	if (hostapd_drv_send_mlme(hapd, &reply, send_len, 0) < 0)
		wpa_printf(MSG_INFO, "Failed to send deauth: %s",
			   strerror(errno));
}

/**
 * Send a beacon/probe-response. This code is
 * borrowed from the BeaconSource element
 * and is modified to retrieve the BSSID/SSID
 * from the sta_mapping_table
 */
void
send_beacon(struct hostapd_data *hapd) {

    fprintf(stderr, "send beacon() start \n");
    char da[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char bssid[6] = {0x00, 0x1b, 0xb3, 0x3c, 0x5a, 0x11};
    char *ssid = "wimaster";
    int actual_length = 0;//beacon的实际构造长度
    int ssid_len = strlen(ssid);
    int i = 0;
    generate_beacon(hapd, da, bssid, ssid, strlen(ssid),
			0, &actual_length);
//    send_deauth(hapd, da, 6);

    return ;
}

void odin_mlme_event_mgmt(struct i802_bss *bss,
			    struct nlattr *freq, struct nlattr *sig,
			    const u8 *frame, size_t len)
{
	int ssi_signal = 0;
	int frame_len = 0;

	frame_len = len;
	if (sig)
		ssi_signal = (s32) nla_get_u32(sig);
    
}

void odin_mlme_event(struct i802_bss *bss,
		       enum nl80211_commands cmd, struct nlattr *frame,
		       struct nlattr *addr, struct nlattr *timed_out,
		       struct nlattr *freq, struct nlattr *ack,
		       struct nlattr *cookie, struct nlattr *sig)
{
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
		odin_mlme_event_mgmt(bss, freq, sig, (const u8*)nla_data(frame),
			nla_len(frame));
	break;
	
	default:
		break;
	}	
}

int odin_process_bss_event(struct nl_msg *msg, void *arg)
{
    fprintf(stderr, "received frame\n");

	struct i802_bss *bss = (struct i802_bss *)arg;
	struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
		
	switch (gnlh->cmd) {
	case NL80211_CMD_FRAME:
	case NL80211_CMD_FRAME_TX_STATUS:
	//need to notice gnlh->cmd is u8 type --nm
		odin_mlme_event(bss, (enum nl80211_commands)gnlh->cmd, tb[NL80211_ATTR_FRAME],
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

int main(int argc, char **argv)
{
    struct hostapd_data *hapd;
    struct hapd_interfaces interfaces;
    struct timeval timeout={10, 0}; //select等待3秒，3秒轮询，要非阻塞就置0 

	fprintf(stderr, "before hostapd_inf_init\n");
    hostapd_inf_init(&interfaces);
	fprintf(stderr, "after hostapd_inf_init\n");
	
    if (interfaces.iface[0] == NULL || interfaces.iface[0]->bss[0] == NULL) {
		//nl80211 driver驱动不可用,可能出现空指针，未赋值
		//wpa_printf(MSG_ERROR, "No hostapd driver wrapper available");
		fprintf(stderr, "read config failed --nm\n");
		return ;
    }
	hapd = interfaces.iface[0]->bss[0];//一个hostapd_data代表一个基本服务集合

	fprintf(stderr, "before hostapd_driver_init\n");
	hostapd_driver_init(interfaces.iface[0],odin_process_bss_event);
	fprintf(stderr, "after hostapd_driver_init, before hostapd_setup_interface\n");


    hostapd_setup_interface(interfaces.iface[0]);
	fprintf(stderr, "odin hostapd_setup_interface --nm\n");

	struct i802_bss *bss = (struct i802_bss *)hapd->drv_priv;
	int res = -1;
	if(!bss || !bss->nl_mgmt || !bss->nl_cb)
		return 0;

    int frame_sock = nl_socket_get_fd(bss -> nl_mgmt);
    int frame_sock_flags = fcntl(frame_sock, F_GETFL, 0);       //获取文件的flags值。
    fcntl(frame_sock, F_SETFL, frame_sock_flags | O_NONBLOCK);   //设置成非阻塞模式；

    int maxfdp1;
    fd_set rset;

    FD_ZERO(&rset);
    while(1) {
        FD_SET(frame_sock, &rset);
        maxfdp1 = frame_sock + 1;
        switch (select(maxfdp1, &rset, NULL, NULL, &timeout))
        {
        case -1: 
            printf("select error\n");
            break;
        case 0:
            fprintf(stderr, "timeout , send beacon()\n");
            send_beacon(hapd);
            fprintf(stderr, "ending, send beacon()\n");
            break;
        default:
            if(FD_ISSET(frame_sock, &rset)) {
                fprintf(stderr, "before nl_recvmsgs\n");
                res = nl_recvmsgs(bss->nl_mgmt, bss->nl_cb);
                if (res < 0) {
                    fprintf(stderr, "nl80211: %s->nl_recvmsgs failed: %d\n",
                       __func__, res);
                }	
                fprintf(stderr, "after nl_recvmsgs\n");
            }
            break;
        }
    }
}
