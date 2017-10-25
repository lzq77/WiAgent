#ifndef RSSI_HANDLER_H
#define RSSI_HANDLER_H

enum rssi_filter_oper {
    FILTER_ADD_STA,
    FILTER_SUB_STA
};

void wiagent_rssi_handle(int fd, short what, void *arg);

void update_rssi_filter(enum rssi_filter_oper oper, const u8 *sta);


#endif /*RSSI_HANDLER_H*/
