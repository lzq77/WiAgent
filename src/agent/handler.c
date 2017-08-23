
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                             // for memset
#include <assert.h>

#include "../utils/common.h"
#include "../utils/wi_event.h"
#include "../ap/wi_vap.h"
#include "../ap/config_file.h"
#include "subscription.h"
#include "handler.h"

#define CONTROLLER_READ "READ"
#define CONTROLLER_WRITE "WRITE"

static struct bufferevent *bev;

/**
 * The callback function for the connection request of the controller,
 * and new a socket to receive the command of the controller.
 */
void control_listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
	bev = wi_bufferevent_socket_new(fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		wpa_printf(MSG_ERROR, "Error constructing bufferevent!\n");
		return;
	}

	bufferevent_setcb(bev, control_readcb, NULL, control_eventcb, NULL);
	bufferevent_enable(bev, EV_READ);
}

void control_readcb(struct bufferevent *bev, void *data)
{
    char read_buf[2048]={0};
    bufferevent_read(bev,read_buf,sizeof(read_buf));
   
    parse_readcb_data(bev, read_buf);
}

void control_eventcb(struct bufferevent *bev, short events, void *user_data)
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

void read_handler(struct bufferevent *bev, char* arg) 
{
    char *write_str = "DATA 0\n";
    printf("read_handler: %s\n", arg);
    bufferevent_write(bev, write_str, strlen(write_str));
}

static void handler_add_vap(char *data[], int size)
{
    struct vap_data *vap;
    u8 addr[6];
    u8 bssid[6];

    assert(data != NULL);

    if (hwaddr_aton(data[0], addr) < 0) {
        wpa_printf(MSG_WARN, "handler_add_vap convert string to MAC address failed!\n");
        return;
    }
    if (hwaddr_aton(data[2], bssid) < 0) {
        wpa_printf(MSG_WARN, "handler_add_vap convert string to MAC address failed!\n");
        return;
    }

    vap = wi_vap_add(addr, bssid, data[3]);
    if(vap == NULL) {
        wpa_printf(MSG_WARN, "handler_add_vap cannot add vap!\n");
        return;
    }
    inet_aton(data[1], &vap->ipv4);
}

static void handler_remove_vap(char *data[], int size)
{
    u8 addr[6];

    assert(data != NULL);

    if (hwaddr_aton(data[0], addr) < 0) {
        wpa_printf(MSG_WARN, "handler_remove_vap convert string to MAC address failed!\n");
        return;
    }

    if (wi_vap_remove(addr) == 0)
        wpa_printf(MSG_DEBUG, "handler_remove_vap remove vap(%s) success!\n", data[0]);
}

#define SUBSCRIPTION_PARAMS_NUM 6

static void handler_subscriptions(char *data[], int size)
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
       
        index++;
        if (strcmp(data[index], "*") == 0) {
            int i = 0;
            for(; i < 6; i++) 
                sub->sta_addr[i] = 0x0;
        }
        else if (hwaddr_aton(data[index], sub->sta_addr) < 0)
            goto fail;

        strcpy(sub->statistic, data[index++]);
        sub->rel = atoi(data[index++]);
        sub->val = strtod(data[index++], NULL);
        add_subscription(sub);
    }
    return;

fail:
    os_free(sub);
    wpa_printf(MSG_WARN, "subscription data format error.\n");
    return;
}

#define WRITE_ARGS_MAX 12

void write_handler(char* data)
{
    char *delim = ".";
    char *command;
    char *array[WRITE_ARGS_MAX];
    short size = 0; 

    wpa_printf(MSG_DEBUG, "write_handler: %s\n", data);
    
    /**
     * Parsing write_handler string, the format is: "module.action mac ip bssid ssid\r\n"
     * for example: "odinagent.add_vap 58:7F:66:DA:81:7C 0.0.0.0 00:1B:B3:DA:81:7C wimaster\r\n"
     */
    command = strsep(&data, delim);
    if (strcmp(command, "odinagent") != 0) {
        return;
    } 

    delim = " \r\n";
    for (command = strsep(&data, delim); command != NULL;
            command = strsep(&data, delim)) {
        array[size] = (char *)os_zalloc(strlen(command) + 1);
        strcpy(array[size], command);
        size++;
    }

    if (strcmp(array[0], "add_vap") == 0)
        handler_add_vap(&array[1], size);
    else if (strcmp(array[0], "remove_vap") == 0)
        handler_remove_vap(&array[1], size);
    else if (strcmp(array[0], "subscriptions") == 0)
        handler_subscriptions(&array[1], size);

    for(;size > 0; size--) {
        os_free(array[size - 1]);
    }
}

void parse_readcb_data(struct bufferevent *bev, char* data)
{
    char *delim = " ";
    char *command;

    command = strsep(&data, delim);  
    if (strcmp(command, CONTROLLER_READ) == 0) {
        read_handler(bev, data);
    }
    else if (strcmp(command, CONTROLLER_WRITE) == 0) {
        write_handler(data);
    }
}


