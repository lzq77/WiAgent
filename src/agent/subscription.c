
#include "../utils/common.h"
#include "../ap/hostapd.h"
#include "../drivers/driver.h"
#include "../utils/wimaster_event.h"
#include "subscription.h"

static struct subscription *list_head = NULL;
static struct subscription *list_tail = NULL;

static void list_subscription()
{
    struct subscription *list = list_head;
    int num = 0;

    wpa_printf(MSG_DEBUG, "\n wimaster subscriptions list:\n");
    while (list) {
        wpa_printf(MSG_DEBUG, "%d. %d "MACSTR" %s %d %lf\n", 
                    ++num, list->id, MAC2STR(list->sta_addr), list->statistic, 
                    list->rel, list->val);
        list = list->next;
    }

}

static void get_signal_strength(evutil_socket_t fd, short what, void *arg)
{
    struct hostap_sta_list sta_list_head;
    struct hostap_sta_list *list_temp;
    struct hostap_sta_list *list_temp_prev;
    struct hostapd_data *hapd = (struct hostapd_data *)arg;
    sta_list_head.next = NULL;
   
    fprintf(stderr, "-------> signal debug: %s start ... \n", __func__);
    
    hostapd_read_all_sta_data(hapd, &sta_list_head);
    
    wpa_printf(MSG_DEBUG, "\nsta signal strength:\n");
    
    list_temp = sta_list_head.next;
    while(list_temp) {
        wpa_printf(MSG_DEBUG, "station("MACSTR") rssi(%d) connected_time(%d)", 
                        MAC2STR(list_temp->sta_addr), list_temp->sta_data->last_rssi, 
                        list_temp->sta_data->connected_msec);
        list_temp_prev = list_temp;
        list_temp = list_temp->next;
        os_free(list_temp_prev->sta_data);
        os_free(list_temp_prev);
        list_temp_prev = NULL;
    }
}

void handle_signal_strength(struct hostapd_data *hapd)
{
    struct event *ev_signal;
    struct timeval tv_signal;

    ev_signal = wimaster_event_new(-1, EV_TIMEOUT | EV_PERSIST, 
            get_signal_strength, hapd);
	tv_signal.tv_sec = 1;
    tv_signal.tv_usec = 0;
	wimaster_event_add(ev_signal, &tv_signal);
}

void add_subscription(struct hostapd_data *hapd, struct subscription *sub)
{
    if (list_tail) {
        list_tail->next = sub;
        list_tail = sub;
        sub->next = NULL;
    }
    else {
        list_tail = list_head = sub;
        sub->next = NULL;
    }
    handle_signal_strength(hapd);
}

