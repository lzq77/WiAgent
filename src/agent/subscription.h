#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

enum relation_t {
    EQUALS, GREATER_THAN, LESSER_THAN
};

struct subscription {
    int id;
    u8 sta_addr[6];
    char statistic[32];
    int rel;
    double val;

    struct subscription *next;
};

void add_subscription(struct subscription *sub);

void remove_subscription(int sub_id);

void clear_subscriptions();

struct subscription * get_subscription(int sub_id);

#endif
