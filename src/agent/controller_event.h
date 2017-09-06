#ifndef CONTROLLER_EVENT_H
#define CONTROLLER_EVENT_H

#include <event2/bufferevent.h>
#include <event2/listener.h>

#define PUSH_PORT 2819
#define CONTROL_PORT 6777

/**
 * 
 */
int controller_event_init(struct hostapd_data *hapd, char *controller_ip);

/**
 * Handles the connection from the SDWN controller.
 */
void controller_listener_cb(struct evconnlistener *listener, 
        evutil_socket_t fd,
        struct sockaddr *sa, 
        int socklen, void *user_data);

void controller_eventcb(struct bufferevent *bev, 
        short events, void *user_data);
#endif

