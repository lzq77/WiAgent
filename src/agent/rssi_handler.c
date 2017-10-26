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
#include "push.h"
#include "vap.h"
#include "rssi_handler.h"

//declared at ../sniffer/wicap.c
extern pthread_mutex_t rssi_file_mutex; 
extern bool is_filter_update;
extern char filter_exp[1024];

static bool rssi_thread_running = false;

struct rssi_entry {
   u8 addr[ETH_ALEN];
   int rssi_sum;
   int rssi_num;
   struct rssi_entry *next;
};

#define RSSI_HASH_SIZE 256
#define RSSI_HASH(addr) (addr[5])

static struct rssi_entry *buckets[RSSI_HASH_SIZE] = { NULL };

static struct rssi_entry * get_rssi_entry(const u8 *addr)
{
    struct rssi_entry *entry;

    entry = buckets[RSSI_HASH(addr)];
    while (entry != NULL && memcmp(entry->addr, addr, 6) != 0)
		entry = entry->next;
    
    return entry;
}

static struct rssi_entry * add_rssi_entry(const u8 *addr)
{
    struct rssi_entry *temp;
    struct rssi_entry *entry;
    
    if (get_rssi_entry(addr) != NULL) {
        wpa_printf(MSG_DEBUG, "Rssi entry "MACSTR" is already existed on hashtable.",
                MAC2STR(addr));
        return;
    }
    
    entry = (struct rssi_entry *)malloc(sizeof(struct rssi_entry));
    memcpy(entry->addr, addr, ETH_ALEN);
    entry->next = NULL;

    if (buckets[RSSI_HASH(addr)] == NULL) {
        buckets[RSSI_HASH(addr)] = entry;
    }
    else {
        for (temp = buckets[RSSI_HASH(addr)]; 
                temp->next != NULL; temp = temp->next);
        temp->next = entry;
    }
    return entry;
}

static void remove_rssi_entry(const u8 *addr)
{
    struct rssi_entry *entry;
    struct rssi_entry *prev;
    
    entry = buckets[RSSI_HASH(addr)];
    prev = entry;
    while (entry != NULL) {
        if (memcmp(entry->addr, addr, 6) == 0)
            break;
        prev = entry;
        entry = entry->next;
    }
    if (entry == NULL)
        return;
    if (entry == prev) {
        buckets[RSSI_HASH(addr)] = entry->next;
        free(entry);
    }
    else {
        prev->next = entry->next;
        free(entry);
    }
}

static void tranverse_rssi_entry_buckets()
{
    struct rssi_entry *entry;
    int i;
    
    for (i = 0; i < RSSI_HASH_SIZE; i++ ) {
        entry = buckets[i];
        if (entry != NULL)
            fprintf(stdout, "----- bucktes %d -----\n", i);
        for (; entry != NULL; entry = entry->next) {
            fprintf(stdout, "  "MACSTR"  \n", MAC2STR(entry->addr));
        }
    }
}

#define MAC_STR_LENGTH 17

static void set_entry_by_express(const char *express)
{
    u8 addr[6];
    char *token;
    char *running;
    char *delim = " ";
    char str[1024];

    strcpy(str, express);
    running = str;
    for (token = strsep(&running, delim); token != NULL;
            token = strsep(&running, delim)) {
        if (strlen(token) == MAC_STR_LENGTH) {
            if (hwaddr_aton(token, addr) == 0) {
                add_rssi_entry(addr);
            }
        }
    }
}

void update_rssi_filter_express(const char *express)
{
    int res;
    pthread_t t_id;

    if (express == NULL || strcmp(express, "") == 0) {
        wpa_printf(MSG_DEBUG, "The rssi filter express is null or empty");
        return;
    }
    set_entry_by_express(express);
    strcpy(filter_exp, express);

    if (!rssi_thread_running) {
        /**
         * New a thread that using libpcap to capture packets
         * and extract rssi value.
         */
        res = pthread_create(&t_id, NULL, wicap, filter_exp);
        if(res != 0) {
            wpa_printf(MSG_ERROR, "%s: %s\n",__func__, strerror(res));
            return;
        }
        rssi_thread_running = true;
    }
    else {
        is_filter_update = true;
        wpa_printf(MSG_DEBUG, "Update rssi filter express : \n  %s\n", express);
    }

        
}


static void construct_rssi_filter_express(char *express)
{
    struct rssi_entry *entry;
    int i;
    int entry_num;
    char mac[18];
    char str[1024];

    strcpy(str, "ether src ");
    
    for (i = 0; i < RSSI_HASH_SIZE; i++) {
        entry = buckets[i];
        for (; entry != NULL; entry = entry->next) {
            sprintf(mac, MACSTR, MAC2STR(entry->addr));
            if (entry_num != 0) {
                strcat(str, " or ");
            }
            strcat(str, mac);
            entry_num++;
        }
    }
    strcpy(express, str);
}

void update_rssi_filter(enum rssi_filter_oper oper, const u8 *sta)
{
    if (oper == FILTER_ADD_STA) {
       if (get_rssi_entry(sta) != NULL)
           return;
       add_rssi_entry(sta);
    }
    else if (oper == FILTER_SUB_STA) {
       remove_rssi_entry(sta);
    }
    construct_rssi_filter_express(filter_exp);
    is_filter_update = true;
}

static void push_rssi_value()
{
    struct rssi_entry *entry;
    int i = 0;
    int rssi_value = 0;
    
    for (; i < RSSI_HASH_SIZE; i++ ) {
        entry = buckets[i];
        for (; entry != NULL; entry = entry->next) {
            if (entry->rssi_num != 0 && entry->rssi_sum !=0) {
                rssi_value = entry->rssi_sum / entry->rssi_num;
                push_subscription(entry->addr, 1, 1, rssi_value + 100);
                entry->rssi_num = 0;
                entry->rssi_sum = 0;
            }
        }
    }
}

static void parse_sta_rssi(char *file_name)
{
    FILE *rssi_file_fd;
    struct rssi_info rinfo;
    struct rssi_entry *entry;

    rssi_file_fd = fopen(file_name, "rb");
    if (rssi_file_fd != NULL) {
        while (!feof(rssi_file_fd)) {
            fread(&rinfo, sizeof(struct rssi_info), 1, rssi_file_fd);
            entry = get_rssi_entry(rinfo.src);
            if (entry == NULL) continue;
            entry->rssi_sum += rinfo.rssi;
            entry->rssi_num++;
        }
        fclose(rssi_file_fd);

        //Emptys file content.
        rssi_file_fd = fopen(file_name, "w");
        fclose(rssi_file_fd);
    }
}

void wiagent_rssi_handle(int fd, short what, void *arg)
{
    char *rssi_file_name = (char *)arg;

    if (rssi_thread_running) {
        /**
         * Uses mutex to keep the sync with sniffer thread 
         * on rssi file operations.
         */
        pthread_mutex_lock(&rssi_file_mutex);
        parse_sta_rssi(rssi_file_name);
        pthread_mutex_unlock(&rssi_file_mutex);
        push_rssi_value();
    }
}
