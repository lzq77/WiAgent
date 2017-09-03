#ifndef WIMASTER_VAP_H
#define WIMASTER_VAP_H

#include <netinet/in.h>         //struct in_addr
#include <time.h>

#include "../ap/hostapd.h"
#include "../utils/common.h"

#define CLEANER_SECONDS 30

struct vap_data {
    u8 bssid[ETH_ALEN];
    u8 addr[ETH_ALEN];
    struct in_addr ipv4;
    char *ssid;
    int ssid_len;
    int is_beacon;
    time_t connected_time;
    
    u8 *beacon_data;
    int beacon_len;
    struct sta_info *sta;    
    
    struct vap_data *next;
};

void wimaster_vap_cleaner(int fd, short what, void *arg);

struct vap_data * wimaster_vap_add(const u8 *bss_addr, 
        const u8 *addr, const u8 *bssid, const char *ssid);

struct vap_data * wimaster_get_vap(const u8 *addr);

void wimaster_for_each_vap(void (*cb)(struct vap_data *vap, void *ctx), void *ctx);

int wimaster_vap_remove(const u8 *bss_addr, const u8 *addr);

#endif
