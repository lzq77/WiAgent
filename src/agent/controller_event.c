/*
 * Processing data from the controller.
 * Copyright (c) 2017 liyaming <liyaming1994@gmail.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                 // for memset
#include <assert.h>

#include <sys/socket.h>             //for inet_aton(...)
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils/common.h"
#include "../utils/wiagent_event.h"
#include "../ap/hostapd.h"
#include "../ap/config_file.h"
#include "../ap/beacon.h"
#include "vap.h"
#include "push.h"
#include "stainfo_handler.h"
#include "rssi_handler.h"
#include "subscription.h"
#include "controller_event.h"

static struct bufferevent *bev;

typedef int (*event_handler)(struct hostapd_data *hapd, const char *sta_mac, 
            const char *data, char *buf);

enum controller_event_type {
    CREAD, CWRITE
};

#define EVENT_NAME_LENGTH 30

struct controller_event {
    char name[EVENT_NAME_LENGTH + 1];
    enum controller_event_type type;
    event_handler handler;
    struct controller_event *next;
};

static struct controller_event *event_head;

void add_controller_event(char *name, enum controller_event_type type,
        event_handler handler) {
    struct controller_event *event;
    struct controller_event *event_temp;
    int name_length;

    event = malloc(sizeof(struct controller_event));
    if (event == NULL) {
        wpa_printf(MSG_ERROR, "%s - memory allocation failed.", __func__);
        return;
    }
    
    name_length = strlen(name) < EVENT_NAME_LENGTH ? strlen(name) : EVENT_NAME_LENGTH;
    strncpy(event->name, name, name_length);
    event->name[name_length] = '\0';
    
    event->type = type;
    event->handler = handler;

    if (event_head == NULL) {
        event_head = event;
        event->next = NULL;
    }
    else {
        event_temp = event_head;
        while (event_temp->next != NULL)
            event_temp = event_temp->next;
        event_temp->next = event;
        event->next = NULL;
    }
}

#define CEVENT_ADD(name, type)\
    add_controller_event(#name, type, controller_##name);

#define CEVENT_MAX_ARGS 8

static int parse_string_to_array(const char *data, char *array[], int num)
{
    char *_info, *info;
    char *delim;
    char *element;
    int i;

    _info = malloc(strlen(data) + 1);
    if (_info == NULL) {
        wpa_printf(MSG_ERROR, "%s - memory allocation failed.", __func__);
        exit(1);
    }
    strcpy(_info, data);

    delim = " ";
    info = _info;
    for(i = 0; (element = strsep(&info, delim)) != NULL && i < num; i++) {
        array[i] = malloc(strlen(element) + 1);
        if (array[i] == NULL) {
            wpa_printf(MSG_ERROR, "%s - memory allocation failed.", __func__);
            exit(1);
        }
        strcpy(array[i], element);
    }

    free(_info);
    return i;
}

static int controller_send_probe_response(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf)
{
    char *probe_infos[CEVENT_MAX_ARGS];
    int infos_num;
    u8 addr[6];
    u8 bssid[6];
    u8 *beacon_data;
    int beacon_len;
	struct wpa_driver_ap_params params;
    struct beacon_settings bs;
    int i;

    infos_num = parse_string_to_array(data, probe_infos, CEVENT_MAX_ARGS);
    if(probe_infos == NULL || infos_num < 2) {
        //printf
        return -1;
    }

    if (hwaddr_aton(sta_mac, addr) < 0 
            || hwaddr_aton(probe_infos[0], bssid) < 0) {
        wpa_printf(MSG_WARN, "%s: convert mac string to MAC address failed.", 
                __func__);
        return -1;
    }

    bs.is_probe = 1;
    bs.is_csa = 0;
    bs.da = addr;
    bs.bssid = bssid;
    bs.ssid = probe_infos[1];
    bs.ssid_len = strlen(probe_infos[1]);

    if (ieee802_11_build_ap_beacon(hapd, &bs, &params) < 0)
        return -1;
    beacon_len = params.head_len + params.tail_len;
    beacon_data = (u8 *)os_zalloc(beacon_len);
    os_memcpy(beacon_data, params.head, params.head_len);
    os_memcpy(beacon_data + params.head_len, params.tail, params.tail_len);
    os_free(params.head);
    os_free(params.tail);

    if (hostapd_drv_send_mlme(hapd, (u8 *)beacon_data, beacon_len, 1) < 0)
		wpa_printf(MSG_DEBUG, "%s: send frame failed.", __func__);

    for (i = 3; i < infos_num; i++) {
        bs.ssid = probe_infos[i];
        bs.ssid_len = strlen(probe_infos[i]);
        
        if (ieee802_11_build_ap_beacon(hapd, &bs, &params) < 0)
            return -1;
        os_memcpy(beacon_data, params.head, params.head_len);
        os_memcpy(beacon_data + params.head_len, params.tail, params.tail_len);
        os_free(params.head);
        os_free(params.tail);

        if (hostapd_drv_send_mlme(hapd, (u8 *)beacon_data, beacon_len, 1) < 0)
            wpa_printf(MSG_DEBUG, "%s: send frame failed.", __func__);

    }
    os_free(beacon_data);
    for(;infos_num > 0; infos_num--)
        free(probe_infos[infos_num-1]);

    return 0;
}

static int controller_add_vap(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf)
{
    char *vap_info, *info;
    struct vap_data *vap;
    u8 addr[6];
    u8 bssid[6];
    char *delim;
    char array[4][20];
    char *element;
    int i;

    if (hwaddr_aton(sta_mac, addr) < 0) {
        wpa_printf(MSG_WARN, "%s - convert string %s to MAC address failed.",
                __func__, sta_mac);
        return -1;
    }

    vap_info = malloc(strlen(data) + 1);
    if (vap_info == NULL) {
        wpa_printf(MSG_ERROR, "%s - memory allocation failed.", __func__);
        return -1;
    }
    strcpy(vap_info, data);

    delim = " ";
    info = vap_info;
    for(i = 0; i < 4; i++) {
        if ((element = strsep(&info, delim)) == NULL)
            break;
        strcpy(array[i], element);
    }

    if (hwaddr_aton(array[1], bssid) < 0) {
        wpa_printf(MSG_WARN, "%s - convert string %s to MAC address failed!", 
                __func__, array[1]);
        return -1;
    }

    //Based on the vap information, add a new vap to the vap list.
    vap = wiagent_vap_add(hapd->own_addr, addr, bssid, array[2]);
    if(vap == NULL) {
        wpa_printf(MSG_WARN, "handler_add_vap cannot add vap!");
        return -1;
    }
    inet_aton(array[0], &vap->ipv4);
    vap->is_beacon = atoi(array[3]);

    wpa_printf(MSG_DEBUG, "Add a vap bssid "MACSTR" sta "MACSTR" ssid %s.", 
                    MAC2STR(vap->bssid), MAC2STR(vap->addr), vap->ssid);

    free(vap_info);

    return 0;
}

static int controller_remove_vap(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf)
{
    u8 addr[6];

    if (hwaddr_aton(sta_mac, addr) < 0) {
        wpa_printf(MSG_WARN, "%s - convert string %s to MAC address failed.", __func__, sta_mac);
        return -1;
    }

    if (wiagent_vap_remove(hapd->own_addr, addr) == 0) {
        wpa_printf(MSG_DEBUG, "Have removed sta %s vap.", sta_mac);
        if (wiagent_remove_stainfo(hapd, addr) == 0)
            wpa_printf(MSG_DEBUG, "Have removed sta %s sta_info.", sta_mac);
    }

    return 0;
}

static int controller_add_stainfo(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf)
{
    u8 addr[6];

    if (hwaddr_aton(sta_mac, addr) < 0) {
        wpa_printf(MSG_WARN, "%s: convert string %s to MAC address failed!n", 
                __func__, sta_mac);
        return -1;
    }

    if (wiagent_add_stainfo(hapd, addr, data) < 0) {
        wpa_printf(MSG_WARN, "Fail to add sta %s sta_info.", sta_mac);
        return -1;
    }
    wpa_printf(MSG_DEBUG, "Have added sta %s sta_info.", sta_mac);
    
    return 0;
}

static int controller_switch_channel(struct hostapd_data *hapd,
        const char *sta_mac, const char *data, char *buf)
{
    char *channel_infos[CEVENT_MAX_ARGS];
    int infos_num;
    u8 addr[6];
    struct vap_data *vap;
    u8 *beacon_data;
	struct wpa_driver_ap_params params;
    struct channel_switch_params cs_params;
	int beacon_len = 0;

    if (hwaddr_aton(sta_mac, addr) < 0) {
        wpa_printf(MSG_WARN, "%s - convert string %s to MAC address failed.", 
                __func__, sta_mac);
        return -1;
    }

    vap = wiagent_get_vap(addr);
    if (!vap) {
        wpa_printf(MSG_WARN, "There is no "MACSTR" vap data on wiagent!", MAC2STR(addr));
        return -1;
    }

    infos_num = parse_string_to_array(data, channel_infos, CEVENT_MAX_ARGS);
    if (infos_num < 3) {
        wpa_printf(MSG_WARN, "%s - new channel information is insufficient.", __func__);
        return -1;
    }

    cs_params.cs_mode = (u8)atoi(channel_infos[0]);
    cs_params.channel = (u8)atoi(channel_infos[1]);
    cs_params.cs_count = (u8)atoi(channel_infos[2]);
    for(;infos_num > 0; infos_num--)
        free(channel_infos[infos_num-1]);
    
    struct beacon_settings bs = {
        .da = vap->addr,
        .bssid = vap->bssid,
        .ssid = vap->ssid,
        .ssid_len = vap->ssid_len,
        .is_probe = 0,
        .is_csa = 1,
        .cs_params = cs_params
    };
	
    /**
     * Rebuild the vap's beacon frame, which containing the CSA element.
     */
    if (ieee802_11_build_ap_beacon(hapd, &bs, &params) < 0)
        return -1;
	
    beacon_len = params.head_len + params.tail_len;
	beacon_data = (u8 *)os_zalloc(beacon_len);
	os_memcpy(beacon_data, params.head, params.head_len);
    os_memcpy(beacon_data + params.head_len, params.tail, params.tail_len);
	os_free(params.head);
	os_free(params.tail);
    if (vap->beacon_data)
        os_free(vap->beacon_data);
    vap->beacon_data = beacon_data;
    vap->beacon_len = beacon_len;

    wpa_printf(MSG_DEBUG, "Have transformed sta %s beacon to csa beacon, csa \
                parameter is (%d %d %d).", sta_mac, cs_params.cs_mode,
                cs_params.channel, cs_params.cs_count);

    return 0;
}

static int controller_rssi_filter_express(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf) {
	update_rssi_filter_express(data);
    return 0;
}

static int controller_read_aggregation_rx(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf)
{
    FILE *fp;
    char path[128];
    char c;
    int i;
    
    wpa_printf(MSG_ERROR, "%s start", __func__);

    sprintf(path, "/sys/kernel/debug/ieee80211/phy0/netdev:wlan0/stations/%s/agg_rx", sta_mac);

    if (!(fp = fopen(path, "r"))) {
        wpa_printf(MSG_ERROR, "Cannot open file %s\n", path);
        return -1;
    }
    for (i = 0; (c = fgetc(fp)) != EOF; i++) {
        buf[i] = c;
    }
    buf[i] = '\0';
    wpa_printf(MSG_DEBUG, "Read aggregation rx string - %s", buf);
    fclose(fp);

    return 0;
}

static int controller_write_aggregation_rx(struct hostapd_data *hapd, 
        const char *sta_mac, const char *data, char *buf)
{
    FILE *fp;
    char path[128];

    wpa_printf(MSG_ERROR, "%s start", __func__);
    
    sprintf(path, "/sys/kernel/debug/ieee80211/phy0/netdev:wlan0/stations/%s/agg_rx", sta_mac);
    wpa_printf(MSG_DEBUG, "rx string - %s, path - %s", data, path);
    fp = fopen(path, "w+");
    fprintf(fp, "%s", data);
    fclose(fp);

    return 0;
}

static void handle_event(struct bufferevent *bev, struct hostapd_data *hapd, 
                        const char *rw, const char *name, 
                        const char *sta_mac, const char *data)
{
    struct controller_event *pevent = event_head;
    char read_buf[1024];
    char write_str[1024];
    int res;

    /**
     * Match controller commands to events in the event list.
     */
    while (pevent != NULL) {
        if (pevent->handler == NULL) {
            pevent = pevent->next;
            continue;
        }
        if (pevent->type == 0 && strcmp(rw, "read") == 0) {
            if (strcmp(pevent->name, name) == 0) {
                res = pevent->handler(hapd, sta_mac, data, read_buf);
                if (res == 0) {
                    /**
                     * Because "read", agent need to return data to 
                     * the controller.
                     */
                    sprintf(write_str, "wiagent %s %d\n%s", 
                            name, strlen(read_buf), read_buf);
                    bufferevent_write(bev, write_str, strlen(write_str));
                }
                break;
            }
        }
        else if (pevent->type == 1 && strcmp(rw, "write") == 0) {
            if (strcmp(pevent->name, name) == 0) { 
                pevent->handler(hapd, sta_mac, data, NULL);
                break;
            }
        }
        pevent = pevent->next;
    }
    if (pevent == NULL) {
        wpa_printf(MSG_WARN, "Controller command %s does not exist", name);
        if (strcmp(rw, "read") == 0) {
            /**
             * Because "read", agent need to return data to 
             * the controller.
             */
            sprintf(write_str, "wiagent no_match 0");
            bufferevent_write(bev, write_str, strlen(write_str));
        }
    }
}

#define ARG_NUMS 4

static void parse_read_cb_string(struct bufferevent *bev, 
                                struct hostapd_data *hapd, char* data)
{
    char *args[ARG_NUMS];
    char *arg;
    int i;
    char *delim = " ";

    for (i = 0; i < (ARG_NUMS-1); i++) {
        arg = strsep(&data, delim);
        if (arg == NULL) {
            wpa_printf(MSG_WARN, "%s - the handle string \"%s\" from \
                    controller is incomplete.", __func__, data);
            return;
        }
        args[i] = (char *)os_zalloc(strlen(arg) + 1);
        strcpy(args[i], arg);
    }
    if (data != NULL) {
        args[i] = (char *)os_zalloc(strlen(data) + 1);
        strcpy(args[i], data);
        i++;
    }

    handle_event(bev, hapd, args[0], args[1], args[2], data ? args[3] : NULL);

    for(; i > 0; i--) {
        os_free(args[i-1]);
    }
}

/**
 * Callback function that handles the data received from controller.
 */
static void event_read_cb(struct bufferevent *bev, struct hostapd_data *hapd)
{
    char *delim;
    char *line;
    char *cur;
    char read_buf[2048]={0};
    
    bufferevent_read(bev,read_buf,sizeof(read_buf));

    /**
     * Read and process every row of data.
     */
    cur = read_buf;
    delim = "\r\n";
    for (line = strsep(&cur, delim); line != NULL;
            line = strsep(&cur, delim)) {
        if (strcmp(line, "") == 0)
            continue;
        wpa_printf(MSG_MSGDUMP, "Controller handle : %s.", line);
        parse_read_cb_string(bev, hapd, line);
    }
}

/**
 * Callback function that handles errors of 
 * the socket connection with controller.
 */
static void event_error_cb(struct bufferevent *bev, short events, 
        void *user_data)
{
	if (events & BEV_EVENT_EOF) {
		wpa_printf(MSG_INFO, "Controller event callback, connection closed");
	} else if (events & BEV_EVENT_ERROR) {
        wpa_printf(MSG_INFO, "Controller event callback,\ 
                got an error on the connection");
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts. 
     */
	bufferevent_free(bev);
}

void run_sniffer(struct hostapd_data *hapd, const char *interface)
{
    struct event *ev_rssi;
	struct timeval tv_rssi;
    ev_rssi = wiagent_event_new(-1, EV_TIMEOUT | EV_PERSIST, 
                wiagent_rssi_handle, NULL);
    tv_rssi.tv_sec = 1;
    tv_rssi.tv_usec = 0;
    wiagent_event_add(ev_rssi, &tv_rssi);

    set_sniffer_interface(interface);
}

/**
 * News a socket to receive the data from controller,
 * and sets the callback function for handling received data or 
 * errors of the socket connection.
 */
void controller_listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
    struct hostapd_data *hapd = (struct hostapd_data *)user_data;

	bev = wiagent_bufferevent_socket_new(fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		wpa_printf(MSG_ERROR, "Error constructing bufferevent");
		return;
	}

    bufferevent_setcb(bev, event_read_cb, NULL, event_error_cb, hapd);
	bufferevent_enable(bev, EV_READ);
}

int controller_event_init(struct hostapd_data *hapd, const char *controller_ip)
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
     * Listen to the controller's connection request and
     * establist a tcp socket to pass the controller's
     * commands.
     */
    memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(CONTROL_PORT);
    controller_listener = wiagent_evconnlistener_new_bind(controller_listener_cb, hapd,
	    LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
	    (struct sockaddr*)&sin, sizeof(sin));
    
    if (!controller_listener) {
		wpa_printf(MSG_ERROR, "Could not create a controller's listener.");
		return -1;
	}
    
    /*
     * Timing send a ping message to controller.
     */
	memset(&push_addr, 0, sizeof(push_addr));
	push_addr.sin_family = AF_INET;
	if(inet_pton(push_addr.sin_family, controller_ip, &push_addr.sin_addr) != 1)
	{
		wpa_printf(MSG_ERROR, "Controller's ip address error.");
		return -1;
	}
	push_addr.sin_port = htons(PUSH_PORT);

    push_sock = socket(AF_INET, SOCK_DGRAM, 0);   //Using UDP socket.
    if (push_sock < 0) {
        wpa_printf(MSG_ERROR, "Failed to create Ping udp socket.");  
        return -1;
    }
    if(connect(push_sock, (struct sockaddr*)&push_addr, sizeof(push_addr) ) < 0) {
		wpa_printf(MSG_ERROR, "Error on connecting the controller with udp socket.");
		return -1;
	}
    
    /**
     * Add push_sock to event, send heartbeat packet regularly.
     */
    ev_ping = wiagent_event_new(push_sock, EV_TIMEOUT | EV_PERSIST, 
            ping_timer, NULL);
	tv_ping.tv_sec = 2;
    tv_ping.tv_usec = 0;
	wiagent_event_add(ev_ping, &tv_ping);

    ev_vap_cleaner = wiagent_event_new(push_sock, EV_TIMEOUT | EV_PERSIST, 
            wiagent_vap_clean, hapd->own_addr);
	tv_vap_cleaner.tv_sec = CLEANER_SECONDS;
    tv_vap_cleaner.tv_usec = 0;
	wiagent_event_add(ev_vap_cleaner, &tv_vap_cleaner);

    CEVENT_ADD(add_stainfo, CWRITE);
    CEVENT_ADD(add_vap, CWRITE);
    CEVENT_ADD(remove_vap, CWRITE);
    CEVENT_ADD(switch_channel, CWRITE);
    CEVENT_ADD(send_probe_response, CWRITE);
    CEVENT_ADD(write_aggregation_rx, CWRITE);
	CEVENT_ADD(rssi_filter_express, CWRITE);
    CEVENT_ADD(read_aggregation_rx, CREAD);

    return 0;
}
