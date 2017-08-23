#include "common.h"
#include "wi_event.h"

static struct event_base *base = NULL;

static inline void check_event_base()
{
    if(!base) {
        base = event_base_new();
        if(!base) {
            wpa_printf(MSG_ERROR, "Could not initialize libevent!\n");
            exit(1);
        }
    }
}

struct event *wi_event_new(evutil_socket_t fd, short events, 
        void (*cb)(evutil_socket_t, short, void *), void *arg)
{
    check_event_base();
    return event_new(base, fd, events, cb, arg);
}

struct evconnlistener *wi_evconnlistener_new_bind(evconnlistener_cb cb, 
        void *ptr, unsigned flags, int backlog, 
        const struct sockaddr *sa, int socklen)
{
    check_event_base();

    return evconnlistener_new_bind(base, cb, ptr, flags, backlog, sa, socklen);
}

struct bufferevent *wi_bufferevent_socket_new(evutil_socket_t fd, int options)
{
    check_event_base();
    return bufferevent_socket_new(base, fd, options);
}

int wi_event_add(struct event *ev, const struct timeval *timeout)
{
    return event_add(ev, timeout);
}

int wi_event_dispatch()
{
    event_base_dispatch(base);
    event_base_free(base);
}
