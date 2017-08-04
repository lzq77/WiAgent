#ifndef HANDLER_H
#define HANDLER_H

#include <event2/bufferevent.h>
#include <event2/listener.h>

/**
 * Handles the connection from the SDWN controller.
 */
void control_listener_cb(struct evconnlistener *listener, 
        evutil_socket_t fd,
        struct sockaddr *sa, 
        int socklen, void *user_data);

/**
 * When a command string come from controller is received,
 * this function will be called.
 */
void control_readcb(struct bufferevent *bev, void *data);

void control_eventcb(struct bufferevent *bev, 
        short events, void *user_data);

/**
 * Handles the controller's command "REAd", 
 * and return some data to controller.
 */
void read_handler(struct bufferevent *bev, char* arg);

/**
 * Handles the controller's command "WRITE".
 */
void write_handler(char* arg);

void parse_readcb_data(struct bufferevent *bev, char* data);


#endif

