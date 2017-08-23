
#include "../utils/common.h"
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

void add_subscription(struct subscription *sub)
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
}

