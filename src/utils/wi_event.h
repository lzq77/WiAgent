#ifndef WI_EVENT_H
#define WI_EVENT_H

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

struct event *wi_event_new(evutil_socket_t fd, short events, 
        void (*cb)(evutil_socket_t, short, void *), void *arg);

struct evconnlistener *wi_evconnlistener_new_bind(evconnlistener_cb cb, 
        void *ptr, unsigned flags, int backlog, 
        const struct sockaddr *sa, int socklen);

struct bufferevent *wi_bufferevent_socket_new(evutil_socket_t fd, int options);

int wi_event_add(struct event *ev, const struct timeval *timeout);

int wi_event_dispatch();

#endif
