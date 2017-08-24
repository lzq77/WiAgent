#ifndef HANDLER_H
#define HANDLER_H

#include <event2/bufferevent.h>
#include <event2/listener.h>

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

