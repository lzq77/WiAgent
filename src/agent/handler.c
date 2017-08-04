
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
#include "../ap/wi_vap.h"
#include "../ap/config_file.h"
#include "handler.h"

#define CONTROLLER_READ "READ"
#define CONTROLLER_WRITE "WRITE"


struct bufferevent *bev;

void
control_listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
	struct event_base *base = user_data;

	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}

	bufferevent_setcb(bev, control_readcb, NULL, control_eventcb, NULL);
	bufferevent_enable(bev, EV_READ);
}

void
control_readcb(struct bufferevent *bev, void *data)
{
    char read_buf[2048]={0};
    bufferevent_read(bev,read_buf,sizeof(read_buf));
   
    parse_readcb_data(bev, read_buf);
}

void
control_eventcb(struct bufferevent *bev, short events, void *user_data)
{
    printf("control_eventcb: ");
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection");
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */
	bufferevent_free(bev);
}

void 
read_handler(struct bufferevent *bev, char* arg) 
{
    char *write_str = "DATA 0\n";
    printf("read_handler: %s\n", arg);
    bufferevent_write(bev, write_str, strlen(write_str));
}

static void handler_add_vap(char *data[], int size)
{
    struct vap_data *vap;
    char *arg;
    char *delim = " ";
    u8 addr[6];
    u8 bssid[6];

    assert(data != NULL);

    if (hwaddr_aton(data[0], addr) < 0) {
        fprintf(stderr, "handler_add_vap convert string to MAC address failed!\n");
        return;
    }
    if (hwaddr_aton(data[2], bssid) < 0) {
        fprintf(stderr, "handler_add_vap convert string to MAC address failed!\n");
        return;
    }

    vap = wi_vap_add(addr, bssid, data[3]);
    if(vap == NULL) {
        fprintf(stderr, "handler_add_vap cannot add vap!\n");
        return;
    }
    inet_aton(data[1], &vap->ipv4);
    
    fprintf(stderr, "handler_add_vap success!\n");
}

#define WRITE_ARGS_MAX 12

void write_handler(char* data)
{
    char *delim = ".";
    char *command;
    char *array[WRITE_ARGS_MAX];
    short size = 0; 

    printf("write_handler: %s\n", data);
    
    command = strsep(&data, delim);
    if (strcmp(command, "odinagent") != 0) {
        return;
    } 
    
    printf("write_handler: %s\n", data);

    delim = " ";
    for (command = strsep(&data, delim); command != NULL;
            command = strsep(&data, delim)) {
        printf("for strsep: %s\n", command);
        array[size] = (char *)os_zalloc(strlen(command) + 1);
        strcpy(array[size], command);
        size++;
    }

    printf("write_handler: %s\n", array[0]);

    if(strcmp(array[0], "add_vap") == 0) {
        handler_add_vap(&array[1], size - 1);
    }

    for(;size > 0; size--) {
        os_free(array[size - 1]);

    }
}
void 
parse_readcb_data(struct bufferevent *bev, char* data)
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


