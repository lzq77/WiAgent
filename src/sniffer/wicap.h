#ifndef WICAP_H
#define WICAP_H

#include "../utils/common.h"

struct rssi_info {
	u8 da[6];
    u8 src[6];
	u8 bssid[6];
    int rssi;
};

void wicap(void *dev);

#endif /*WICAP_H*/

