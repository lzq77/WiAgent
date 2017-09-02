
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                             // for memset
#include <assert.h>

#include <sys/socket.h>             //for inet_aton(...)
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils/common.h"
#include "../utils/wimaster_event.h"
#include "../ap/hostapd.h"
#include "../ap/config_file.h"
#include "vap.h"
#include "push.h"
#include "stainfo_handler.h"
#include "subscription.h"
#include "controller_event.h"

#define CONTROLLER_READ "READ"
#define CONTROLLER_WRITE "WRITE"

static struct bufferevent *bev;

int controller_event_init(struct hostapd_data *hapd, char *controller_ip)
{
    /**
     * libevent event.
     */
    struct event *ev_ping;
	struct timeval tv_ping;
    struct event *ev_vap_cleaner;
    struct timeval tv_vap_cleaner;
    struct evconnlistener *controller_listener;
	struct sockaddr_in sin;
    struct sockaddr_in push_addr;
    int push_sock;

    /**
     * Listening controller connection, which is 
     * used to send its control commands.
     */
    memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(CONTROL_PORT);
    controller_listener = wimaster_evconnlistener_new_bind(controller_listener_cb, hapd,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    (struct sockaddr*)&sin, sizeof(sin));
    
    if (!controller_listener) {
		wpa_printf(MSG_ERROR, "Could not create a listener!\n");
		return -1;
	}
    
    /*
     * Timing send a ping message to controller.
     */
	memset(&push_addr, 0, sizeof(push_addr));
	push_addr.sin_family = AF_INET;
	if(inet_pton(push_addr.sin_family, controller_ip, &push_addr.sin_addr) != 1)
	{
		wpa_printf(MSG_ERROR, "controller_ip address error!\n");
		return -1;
	}
	push_addr.sin_port = htons(PUSH_PORT);

    push_sock = socket(AF_INET, SOCK_DGRAM, 0);   //Using UDP socket.
    if (push_sock < 0) {
        wpa_printf(MSG_ERROR, "Create Socket Failed!\n");  
        return -1;
    }
    if(connect(push_sock, (struct sockaddr*)&push_addr, sizeof(push_addr) ) < 0) {
		wpa_printf(MSG_ERROR, "UDP connect error!\n");
		return -1;
	}
    
    /**
     * Add push_sock to event, send heartbeat packet regularly.
     */
    ev_ping = wimaster_event_new(push_sock, EV_TIMEOUT | EV_PERSIST, 
            ping_timer, NULL);
	tv_ping.tv_sec = 2;
    tv_ping.tv_usec = 0;
	wimaster_event_add(ev_ping, &tv_ping);

    /**
     * Add push_sock to event, send heartbeat packet regularly.
     */
    ev_vap_cleaner = wimaster_event_new(push_sock, EV_TIMEOUT | EV_PERSIST, 
            wimaster_vap_cleaner, hapd->own_addr);
	tv_vap_cleaner.tv_sec = CLEANER_SECONDS;
    tv_vap_cleaner.tv_usec = 0;
	wimaster_event_add(ev_vap_cleaner, &tv_vap_cleaner);

    return 0;
}

static void controller_add_vap(struct hostapd_data *hapd, char *data[], int size)
{
    struct vap_data *vap;
    u8 addr[6];
    u8 bssid[6];

    if (hwaddr_aton(data[0], addr) < 0) {
        wpa_printf(MSG_WARN, "%s: convert string %s to MAC address failed!\n", __func__, data[0]);
        return;
    }
    if (hwaddr_aton(data[2], bssid) < 0) {
        wpa_printf(MSG_WARN, "%s: convert string %s to MAC address failed!\n", __func__, data[2]);
        return;
    }

    vap = wimaster_vap_add(hapd->own_addr, addr, bssid, data[3]);
    if(vap == NULL) {
        wpa_printf(MSG_WARN, "handler_add_vap cannot add vap!\n");
        return;
    }
    inet_aton(data[1], &vap->ipv4);
    vap->is_beacon = atoi(data[4]);
    wpa_printf(MSG_DEBUG, "%s - %s - add vap %s success!\n", 
                    __TIME__, __func__, data[0]);

}

static void controller_remove_vap(struct hostapd_data *hapd, char *data[], int size)
{
    u8 addr[6];

    if (hwaddr_aton(data[0], addr) < 0) {
        wpa_printf(MSG_WARN, "handler_remove_vap convert string to MAC address failed!\n");
        return;
    }

    if (wimaster_vap_remove(hapd->own_addr, addr) == 0) {

        if (wimaster_remove_stainfo(hapd, addr) == 0)
            wpa_printf(MSG_DEBUG, "%s - %s - remove (%s) vap and sta_info success!\n", 
                    __TIME__, __func__, data[0]);
    }
}

static void controller_action_csa(struct hostapd_data *hapd,
        char *data[], int size)
{
    u8 addr[6];
    u8 block_tx, new_channel, cs_count;
    struct vap_data *vap;
    int i;

    if (hwaddr_aton(data[0], addr) < 0) {
        wpa_printf(MSG_WARN, " convert string to MAC address failed!\n");
        return;
    }

    vap = wimaster_get_vap(addr);
    if (!vap) {
        wpa_printf(MSG_WARN, "There is no "MACSTR" vap data on wimaster!\n", MAC2STR(addr));
        return;
    }

    block_tx = (u8)atoi(data[1]);
    new_channel = (u8)atoi(data[2]);
    cs_count = (u8)atoi(data[3]);

    for (i = 0; i < 10; i++) {
    if (hostapd_send_csa_action_frame(hapd, addr, 
                vap->bssid, block_tx, new_channel, cs_count) < 0) {
        wpa_printf(MSG_WARN, "controller_action_csa send csa action frame failed.\n");
        return;
    }
    wpa_printf(MSG_WARN, "controller_action_csa send csa action frame repeat %d.\n", i);
    }
}


static void controller_add_stainfo(struct hostapd_data *hapd,
        char *data[], int size)
{
    u8 addr[6];
    if (hwaddr_aton(data[0], addr) < 0) {
        wpa_printf(MSG_WARN, "%s: convert string %s to MAC address failed!\n", __func__, data[0]);
        return;
    }

    if (wimaster_add_stainfo(hapd, addr, data[1]) < 0) {
        wpa_printf(MSG_WARN, "Add sta_info %s failed.\n", data[0]);
        return;
    }
    wpa_printf(MSG_DEBUG, "%s - %s - add sta_info %s success!\n", 
                    __TIME__, __func__, data[0]);

}

#define SUBSCRIPTION_PARAMS_NUM 6

static void controller_subscriptions(struct hostapd_data *hapd, 
        char *data[], int size)
{
    struct subscription *sub;
    int num_rows;
    int index = 0;
    
    if (size < SUBSCRIPTION_PARAMS_NUM) {
        wpa_printf(MSG_WARN, "The number of subscription parameters %d \
                is insufficient.\n", size);
        return;
    }
    //WRITE odinagent.subscriptions 1 1 00:00:00:00:00:00 signal 2 -30.0
    num_rows = atoi(data[index++]);

    /**
     * FIXME: Only one row of data is processed.
     */
    if (num_rows > 0) {
        sub = (struct subscription *)os_zalloc(sizeof(struct subscription));
        sub->id = atoi(data[index++]);
       
        if (strcmp(data[index], "*") == 0) {
            int i = 0;
            for(; i < 6; i++) 
                sub->sta_addr[i] = 0x0;
        }
        else if (hwaddr_aton(data[index], sub->sta_addr) < 0)
            goto fail;

        index++;
        strcpy(sub->statistic, data[index++]);
        sub->rel = atoi(data[index++]);
        sub->val = strtod(data[index++], NULL);
        add_subscription(hapd, sub);
    }
    return;

fail:
    os_free(sub);
    wpa_printf(MSG_WARN, "subscription data format error.\n");
    return;
}

static void handle_read(struct bufferevent *bev, 
                struct hostapd_data *hapd, char* arg) 
{
    char *write_str = "DATA 0\n";
    bufferevent_write(bev, write_str, strlen(write_str));
}


#define WRITE_ARGS_MAX 12

void handle_write(struct hostapd_data *hapd, char* data)
{
    char *delim = ".";
    char *command;
    char *array[WRITE_ARGS_MAX];
    int size = 0; 
    
    /**
     * Parsing write_handler string, the format is: "module.action mac ip bssid ssid\r\n"
     * for example: "odinagent.add_vap 58:7F:66:DA:81:7C 0.0.0.0 00:1B:B3:DA:81:7C wimaster\r\n"
     */
    command = strsep(&data, delim);
    if (strcmp(command, "odinagent") != 0) 
        return;

    delim = " ";
    for (command = strsep(&data, delim); command != NULL;
            command = strsep(&data, delim)) {

        if (strcmp(command, "") == 0)
            continue;

        array[size] = (char *)os_zalloc(strlen(command) + 1);
        strcpy(array[size], command);
        if (size == 1 && strcmp(array[0], "add_station") == 0) {
            array[++size] = (char *)os_zalloc(strlen(data) + 1);
            strcpy(array[size], data);
            break;
        }
        size++;
    }

    if (strcmp(array[0], "add_vap") == 0)
        controller_add_vap(hapd, &array[1], size);
    else if (strcmp(array[0], "remove_vap") == 0)
        controller_remove_vap(hapd, &array[1], size);
    else if (strcmp(array[0], "subscriptions") == 0)
        controller_subscriptions(hapd, &array[1], size);
    else if (strcmp(array[0], "add_station") == 0)
        controller_add_stainfo(hapd, &array[1], size);
    else if (strcmp(array[0], "switch_channel") == 0)
        controller_action_csa(hapd, &array[1], size);

    for(;size > 0; size--) {
        os_free(array[size - 1]);
    }
}

void handle_readcb_data(struct bufferevent *bev, struct hostapd_data *hapd, 
                            char* data)
{
    char *command;
    char *delim = " ";

    command = strsep(&data, delim);  
    if (strcmp(command, CONTROLLER_READ) == 0) {
        handle_read(bev, hapd, data);
    }
    else if (strcmp(command, CONTROLLER_WRITE) == 0) {
        handle_write(hapd, data);
    }
}


void controller_readcb(struct bufferevent *bev, struct hostapd_data *hapd)
{
    char *delim;
    char *line;
    char *cur;
    char read_buf[2048]={0};

    bufferevent_read(bev,read_buf,sizeof(read_buf));

    wpa_printf(MSG_DEBUG, "%s - controller command: %s\n", __TIME__, read_buf);

    /**
     * Read and process every row of data.
     */
    cur = read_buf;
    delim = "\r\n";
    for (line = strsep(&cur, delim); line != NULL;
            line = strsep(&cur, delim)) {
        if (strcmp(line, "") == 0)
            continue;

        handle_readcb_data(bev, hapd, line);
    }
}

void controller_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	if (events & BEV_EVENT_EOF) {
		wpa_printf(MSG_INFO, "Controller event callback, connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		wpa_printf(MSG_INFO, "Controller event callback, got an error on the connection\n");
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */
	bufferevent_free(bev);
}

/**
 * The callback function for the connection request of the controller,
 * and new a socket to receive the command of the controller.
 */
void controller_listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
    struct hostapd_data *hapd = (struct hostapd_data *)user_data;

	bev = wimaster_bufferevent_socket_new(fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		wpa_printf(MSG_ERROR, "Error constructing bufferevent!\n");
		return;
	}

	bufferevent_setcb(bev, controller_readcb, NULL, controller_eventcb, hapd);
	bufferevent_enable(bev, EV_READ);
}

