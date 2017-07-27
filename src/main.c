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

#include "hostapd_mgmt.h"


#include "controller_event.h"
#include "controller_push.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#define PUSH_PORT 2819
#define CONTROL_PORT 6777

static void
wi_mgmt_frame_cb(evutil_socket_t fd, short what, void *arg)
{
    int res;
    struct hostapd_data *hapd = (struct hostapd_data *)arg;
    struct i802_bss *bss = (struct i802_bss *)hapd->drv_priv;
        
    res = nl_recvmsgs(bss->nl_mgmt, bss->nl_cb);
    if (res < 0) {
        fprintf(stderr, "nl80211: %s->nl_recvmsgs failed: %d\n",
            __func__, res);
    }	
}

static void
send_beacon(evutil_socket_t fd, short what, void *arg)
{
    struct hostapd_data *hapd = (struct hostapd_data *)arg;

    char da[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char bssid[6] = {0x00, 0x1b, 0xb3, 0x8b, 0x88, 0xa3};
    char *ssid = "wimaster";
    int ssid_len = strlen(ssid);
    wi_handle_beacon(hapd, da, bssid, ssid, strlen(ssid));

    return;
}

static int
init_hostapd_interface(struct hapd_interfaces *interfaces)
{
	interfaces->config_read_cb = hostapd_config_read;	
	interfaces->global_iface_path = NULL;
	interfaces->global_iface_name = NULL;
	interfaces->global_ctrl_sock = -1;
	
	interfaces->count = 1;   //only using wlan0
	if (interfaces->count) {
	    interfaces->iface = (struct hostapd_iface**)os_calloc(
                interfaces->count,sizeof(struct hostapd_iface *));
		if (interfaces->iface == NULL) {
			wpa_printf(MSG_ERROR, "interfaces->iface malloc failed\n");
			goto out;
		}
	}

	/* Allocate and parse configuration for wlan0 interface files */
    interfaces->iface[0] = hostapd_interface_init(interfaces,"/tmp/run/hostapd-phy0.conf", 0);
	if (!interfaces->iface[0]) {
		wpa_printf(MSG_ERROR, "Failed to initialize interface");
        goto out;
	}
	
    if (interfaces->iface[0] == NULL || interfaces->iface[0]->bss[0] == NULL) {
		//nl80211 driver驱动不可用,可能出现空指针，未赋值
		//wpa_printf(MSG_ERROR, "No hostapd driver wrapper available");
		fprintf(stderr, "read config failed.\n");
		goto out;
    }

    /* Enable configured interfaces. */
	//  hapd = interfaces.iface[0]->bss[0];     //一个hostapd_data代表一个基本服务集合
    if (hostapd_driver_init(interfaces->iface[0], wi_process_bss_event) < 0)
        goto out;
    if (hostapd_setup_interface(interfaces->iface[0]) < 0)
        goto out;

    return 0;

out:
	os_free(interfaces->iface);
    return -1;
}

int main(int argc, char **argv)
{
    struct hapd_interfaces interfaces;
    struct hostapd_data *hapd;
    struct i802_bss *bss;

	struct event_base *base; //定义一个event_base  
    struct event *ev_ping;
    struct event *ev_frame; 
    struct event *ev_beacon;
	struct timeval two_sec;
    struct timeval tv_beacon;

	char *controller_ip;
    unsigned int controller_port;
    struct evconnlistener *control_listener;
	struct sockaddr_in sin;
    struct sockaddr_in push_addr;
    
    int push_sock;  
    int frame_sock;
    int frame_sock_flags;

    if ((controller_ip = *(++argv)) == NULL) {
        fprintf(stderr, "Need controller's ip address.");
        return 1;
    }

    /**
     * Use the libevent as the main frame of the program.
     */
    base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}
    
    memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(CONTROL_PORT);

    /**
     * Listening controller connection, which is 
     * used to send its control commands.
     */
	control_listener = evconnlistener_new_bind(base, control_listener_cb, (void *)base,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    (struct sockaddr*)&sin, sizeof(sin));
    
    if (!control_listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}
    
    /*
     * Timing send a ping message to controller.
     */
	memset(&push_addr, 0, sizeof(push_addr));
	push_addr.sin_family = AF_INET;
	if(inet_pton(push_addr.sin_family, controller_ip, &push_addr.sin_addr) != 1)
	{
		perror( "controller_ip address error!\n");
		return -1;
	}
	push_addr.sin_port = htons(PUSH_PORT);

    push_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (push_sock < 0)
    {
        perror("Create Socket Failed!\n");  
        return -1;
    }
    if(connect(push_sock, (struct sockaddr*)&push_addr, sizeof(push_addr) ) < 0)
	{
		perror("UDP connect error!\n");
		return -1;
	}
    
    //Add push_sock to event.
    ev_ping = event_new(base, push_sock, EV_TIMEOUT | EV_PERSIST, 
            ping_timer, NULL);
	two_sec.tv_sec = 2;
    two_sec.tv_usec = 0;
	event_add(ev_ping, &two_sec);

    /**
     * Initialize the wireless interfaces (wlan0), 
     * the code is transplanted from hostapd.
     */
    os_memset(&interfaces, 0, sizeof(struct hapd_interfaces));
    if (init_hostapd_interface(&interfaces) < 0) { 
        fprintf(stderr, "Initialize the wireless interfaces failed.\n");
        return -1;
    }

    hapd = interfaces.iface[0]->bss[0];
    bss = (struct i802_bss *)hapd->drv_priv;
    if(!bss || !bss->nl_mgmt || !bss->nl_cb) {
        fprintf(stderr, "The bss(basic service set) configures error.\n");
        return 1;
    }

    /**
     * Set the socket fd that receive management frame 
     * to a non-blocking state.
     */
    frame_sock = nl_socket_get_fd(bss->nl_mgmt);
    frame_sock_flags = fcntl(frame_sock, F_GETFL, 0); //获取文件的flags值。
    fcntl(frame_sock, F_SETFL, frame_sock_flags | O_NONBLOCK);   //设置成非阻塞模式；

    
    ev_frame = event_new(base, frame_sock, EV_READ | EV_PERSIST,
            wi_mgmt_frame_cb, hapd);
    event_add(ev_frame, NULL);

    ev_beacon = event_new(base, -1, EV_TIMEOUT | EV_PERSIST, 
            send_beacon, hapd);
	tv_beacon.tv_sec = 0;
    tv_beacon.tv_usec = 200 * 1000;
	event_add(ev_beacon, &tv_beacon);
	
    event_base_dispatch(base);
	event_base_free(base);  
	
	return 0;
}

