#ifndef RSSI_HANDLER_H
#define RSSI_HANDLER_H

#define RSSI_FILE "/tmp/wiagent_rssi"

enum rssi_filter_oper {
    FILTER_ADD_STA,
    FILTER_SUB_STA
};

void wiagent_rssi_handle(int fd, short what, void *arg);

void update_rssi_filter(enum rssi_filter_oper oper, const u8 *sta);
void update_rssi_filter_express(const char *express);

void set_sniffer_interface(const char *interface);
#endif /*RSSI_HANDLER_H*/
