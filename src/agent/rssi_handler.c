/*
 * rssi handler
 * Copyright (c) 2017, liyaming <liyaming1994@gmail.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <stdio.h>
#include <stdlib.h>                     
#include <string.h>                             // for memset
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include "../sniffer/wicap.h"
#include "../utils/common.h"
#include "vap.h"
#include "rssi_handler.h"

//declared at ../sniffer/wicap.c
extern pthread_mutex_t rssi_file_mutex; 
extern int is_filter_update;
extern char filter_exp[1024];

struct filter_address {
    u8 addr[6];
    struct filter_address *next;
};

void update_filter_vap(struct vap_data *vap, void *ctx)
{
    struct filter_address *fa = (struct filter_address *)ctx;
    if (vap->is_beacon) {
        while (fa->next)
            fa = fa->next;
        fa->next = os_malloc(sizeof(struct filter_address));
        fa = fa->next;
        os_memcpy(fa->addr, vap->addr, 6);
        fa->next = NULL;
    }
}

void parse_sta_rssi(char *file_name)
{
    char *rssi_file_name = "/tmp/wiagent_rssi.hex";
    FILE *rssi_file_fd;
    struct rssi_info rinfo;
    int rsum = 0, rnum = 0, ravg = 0;

    rssi_file_fd = fopen(rssi_file_name, "rb");
    if (rssi_file_fd != NULL) {
        while (!feof(rssi_file_fd)) {
            fread(&rinfo, sizeof(struct rssi_info), 1, rssi_file_fd);
            if (rinfo.rssi > 0 || rinfo.rssi < -100)
                continue;
            rsum += rinfo.rssi;
            rnum++;
        }
        if (rnum) {
            ravg = rsum / rnum;
            fprintf(stderr, "Average rssi value (%d), total number (%d)",
                    ravg, rnum);
        }
        fclose(rssi_file_fd);

        //Emptys file content.
        rssi_file_fd = fopen(rssi_file_name, "w");
        fclose(rssi_file_fd);
    } 
}

void wiagent_rssi_handle(int fd, short what, void *arg)
{
    char *rssi_file_name = (char *)arg;
    int res;
    pthread_t t_id;
    struct filter_address fa_head;
    struct filter_address *fa_temp;
    char filter_temp[64];
    static bool rssi_thread_running = false;
    fa_head.next = NULL;

    wiagent_for_each_vap(update_filter_vap, &fa_head);
    if (fa_head.next) {
        sprintf(filter_exp, "ether src "MACSTR, MAC2STR(fa_head.next->addr));
        if (!rssi_thread_running) {
            /**
             * New a thread that using libpcap to capture packets
             * and extract rssi value.
             */
            fprintf(stderr, "<-- rssi debug --> Create a thread -- wicap rssi\n");
            res = pthread_create(&t_id, NULL, wicap, filter_exp);
            if(res != 0) {
                wpa_printf(MSG_ERROR, "%s: %s\n",__func__, strerror(res));
                return;
            }
            rssi_thread_running = true;
        }
        else {
            /**
             * Constructs 802.11 frame filter exprition.
             */
            fa_temp = fa_head.next->next;
            while (fa_temp) {
                sprintf(filter_temp, " or "MACSTR, MAC2STR(fa_temp->addr));
                strcat(filter_exp, filter_temp);
                fa_temp = fa_temp->next;
            }
            is_filter_update = 1;
            fprintf(stderr, "<-- rssi debug --> Update rssi filter expression\n");
            fprintf(stderr, "<-- rssi debug --> %s\n", filter_exp);
        }
    }

    if (rssi_thread_running) {
        pthread_mutex_lock(&rssi_file_mutex);
        fprintf(stderr, "<-- rssi debug --> parse_sta_rssi\n");
        parse_sta_rssi(rssi_file_name);
        pthread_mutex_unlock(&rssi_file_mutex);
    }
}
