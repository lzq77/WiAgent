#ifndef WIMASTER_VAP_H
#define WIMASTER_VAP_H

#include <netinet/in.h>         //struct in_addr

#include "sta_info.h"
#include "hostapd.h"
#include "../utils/common.h"

struct vap_data {

    struct vap_data *next;

    u8 bssid[ETH_ALEN];
    u8 addr[ETH_ALEN];
    struct in_addr ipv4;
    char *ssid;

    struct sta_info *sta;    
};

struct vap_data * wimaster_vap_add(const u8 *addr, const u8 *bssid, const char *ssid);

struct vap_data * wimaster_get_vap(const u8 *addr);

int wimaster_for_each_vap(int (*cb)(struct vap_data *vap, void *ctx), void *ctx);

int wimaster_vap_remove(const u8 *addr);

const char *get_stainfo_json(struct hostapd_data *hapd, const u8 *addr);


#endif
