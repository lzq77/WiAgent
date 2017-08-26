#ifndef WIMASTER_EVENT_H
#define WIMASTER_EVENT_H

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

struct event *wimaster_event_new(evutil_socket_t fd, short events, 
        void (*cb)(evutil_socket_t, short, void *), void *arg);

struct evconnlistener *wimaster_evconnlistener_new_bind(evconnlistener_cb cb, 
        void *ptr, unsigned flags, int backlog, 
        const struct sockaddr *sa, int socklen);

struct bufferevent *wimaster_bufferevent_socket_new(evutil_socket_t fd, int options);

int wimaster_event_add(struct event *ev, const struct timeval *timeout);

int wimaster_event_dispatch(void);

#endif
