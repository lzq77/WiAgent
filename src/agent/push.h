#ifndef PUSH_H
#define PUSH_H

#include "../ap/hostapd.h"

void ping_timer(int fd, short what, void *address);

void push_ping(void);

void wimaster_probe(const u8 *addr, const char *ssid);

void wimaster_station(struct hostapd_data *hapd, const u8 *addr);

#endif
