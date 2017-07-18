
#include "controller_event.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                             // for memset

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

void 
write_handler(char* arg)
{
    printf("write_handler: %s\n", arg);
}

void 
parse_readcb_data(struct bufferevent *bev, char* data)
{
    char *delim = " ";
    char *command;
    char *arg;

    command = strtok(data, delim);  
    if (strcmp(command, CONTROLLER_READ) == 0) {
        arg = (data + strlen(command) + 1);
        read_handler(bev, arg);
    }
    else if (strcmp(command, CONTROLLER_WRITE) == 0) {
        arg = (data + strlen(command) +1);
        write_handler(arg);
    }
}


