#ifndef PUSH_H
#define PUSH_H

#include "../ap/hostapd.h"

void ping_timer(int fd, short what, void *address);

void push_ping(void);

void push_subscription(const u8 *addr, int count, int sub_id, int value);

void wimaster_probe(const u8 *addr, const char *ssid);

void push_stainfo(const u8 *addr, const char *stainfo);

#endif
