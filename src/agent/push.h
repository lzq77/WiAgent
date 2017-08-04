#ifndef PUSH_H
#define PUSH_H

#include "../ap/hostapd.h"

void ping_timer(evutil_socket_t fd, short what, void *address);

void push_ping(void);

void wi_probe(const u8 *addr, const char *ssid);

void wi_station(struct hostapd_data *hapd, const u8 *addr);

#endif
