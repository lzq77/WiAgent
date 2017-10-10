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
#include "rssi_handler.h"

extern pthread_mutex_t rssi_file_mutex; //declared at ../sniffer/wicap.c

void wiagent_rssi_handle(int fd, short what, void *arg)
{
    char *rssi_file_name = (char *)arg;
    FILE *rssi_file_fd;
    struct rssi_info rinfo;
    int res;
    int rsum = 0, rnum = 0, ravg = 0;

    pthread_mutex_lock(&rssi_file_mutex);
    rssi_file_fd = fopen(rssi_file_name, "rb");
    if (rssi_file_fd != NULL) {
        while (!feof(rssi_file_fd)) {
            res = fread(&rinfo, sizeof(struct rssi_info), 1, rssi_file_fd);
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
    pthread_mutex_unlock(&rssi_file_mutex);
}
