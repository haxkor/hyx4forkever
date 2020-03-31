#ifndef HYXWIP_UPDATER_H
#define HYXWIP_UPDATER_H

#include "blob.h"
#include "view.h"

#include <stdint.h>

enum SOCKPAIR_IND {
    FDIND_VIEW=0,
    FDIND_UPDATER=1
};
enum POLL_IND {
    IND_PAULA=0,
    IND_VIEW=1
};


struct update_entry {
    u_int32_t start;
    u_int32_t len;
    //size_t age;
    byte * newdata;


};

struct init_arg {
    struct blob * blob;
    struct view * view;


};
void updater_init(struct view * view);
void *start(void *arg);
void fromMain(short events);
void fromPaula(short events);
void getUpdates_fromPaula();
void setup_sock();









#endif //HYXWIP_UPDATER_H

