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

    hostapd_inf_init(&interfaces);

	if (interfaces.iface[0] == NULL || interfaces.iface[0]->bss[0] == NULL) {
		//nl80211 driver驱动不可用,可能出现空指针，未赋值
		//wpa_printf(MSG_ERROR, "No hostapd driver wrapper available");
		fprintf(stderr, "read config failed --nm\n");
		return ;
    }
	//fprintf(stderr, "read config over11111111111 --nm\n");
	hapd = interfaces.iface[0]->bss[0];//一个hostapd_data代表一个基本服务集合

	hostapd_driver_init(interfaces.iface[0],odin_process_bss_event);


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
        select(maxfdp1, &rset, NULL, NULL, NULL);
        if(FD_ISSET(frame_sock, &rset)) {
            fprintf(stderr, "before nl_recvmsgs\n");
		    res = nl_recvmsgs(bss->nl_mgmt, bss->nl_cb);
		
		    if (res < 0) {
			    fprintf(stderr, "nl80211: %s->nl_recvmsgs failed: %d\n",
				   __func__, res);
		    }	
            fprintf(stderr, "after nl_recvmsgs\n");
        }
    }
}
