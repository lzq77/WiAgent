#ifndef STAINFO_HANDLER_H
#define STAINFO_HANDLER_H

#include "../utils/common.h"
#include "../ap/hostapd.h"

int wimaster_push_stainfo(struct hostapd_data *hapd, const u8 *addr);

int wimaster_add_stainfo(struct hostapd_data *hapd, 
        const u8 *addr, const char *stainfo);

int wimaster_remove_stainfo(struct hostapd_data *hapd, 
        const u8 *addr);

#endif
