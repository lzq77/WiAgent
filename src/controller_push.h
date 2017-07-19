#ifndef CONTROLLER_PUSH_H
#define CONTROLLER_PUSH_H

void ping_timer(evutil_socket_t fd, short what, void *address);

void push_ping(void);

#endif
