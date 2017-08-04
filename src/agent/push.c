#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <sys/stat.h>
#include <netinet/in.h>                         // for sockaddr_in  
#include <sys/types.h>                          // for socket  
#include <sys/socket.h>                         // for socket 

#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                             // for memset
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <json.h>

#include "../utils/common.h"
#include "../ap/wi_vap.h"
#include "push.h"

static int udp_fd;

static int
push(const char *data)
{
    if (data != NULL) {
       if (write(udp_fd, data, strlen(data)) < 0) {
            return -1;
       }
       fprintf(stderr, "push: %s\n", data);
       return 0;
    }
    return -1;
}

#define MAC_STR_LEN 17

#define PING "ping"
#define PROBE "probe"
#define STATION "station"


void
push_ping(void) 
{
    if (push(PING) > 0) {
        printf("ping controller.\n");
    }
}

void 
ping_timer(evutil_socket_t fd, short what, void *address)
{
    static bool first = true;

    if (first) {
        assert(fd > 0);
        udp_fd = fd;
        first = false;
    }
    else {
        push_ping(); 
    }
}


void wi_probe(const u8 *addr, const char *ssid)
{
    assert(addr != NULL);
    char *str;

    if (ssid == NULL) {
        str = (char *)os_zalloc(strlen(PROBE) + 1 + MAC_STR_LEN + 1);
        sprintf(str, PROBE" "MACSTR, MAC2STR(addr));
    }
    else {
        str = (char *)os_zalloc(strlen(PROBE) + 1 
                + MAC_STR_LEN + 1 + strlen(ssid) + 1);
        sprintf(str, PROBE" "MACSTR" %s", MAC2STR(addr), ssid);
    }
    push(str);
    os_free(str);
}

void wi_station(struct hostapd_data *hapd, const u8 *addr)
{
    assert(addr != NULL);
    char *str;
    const char *stainfo_json;

    stainfo_json = get_stainfo_json(hapd, addr);
    if (stainfo_json) {
        str = (char *)os_zalloc(strlen(STATION) + 1 + MAC_STR_LEN + 1 
                        + strlen(stainfo_json) + 1);
        sprintf(str, STATION" "MACSTR" %s", MAC2STR(addr), stainfo_json);
        push(str);
        os_free(str);
    }
    else {
        wpa_printf(MSG_INFO, "sta_info is null, unable to push to controller.\n");
    }
}

