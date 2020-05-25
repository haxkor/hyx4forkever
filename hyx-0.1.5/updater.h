#ifndef HYXWIP_UPDATER_H
#define HYXWIP_UPDATER_H

#include "blob.h"
#include "view.h"

#include <stdint.h>

enum SOCKPAIR_IND {
    FDIND_VIEW = 0,
    FDIND_UPDATER = 1
};
enum POLL_IND {
    IND_PAULA = 0,
    IND_VIEW = 1
};
enum TRANSMISSION_TYPE {
    UPD_FROMBLOB = 0x40,
    UPD_FROMBLOBNEXT = 0x41,
    UPD_FROMPAULA = 1,
    UPD_FROMPAULAINSERT = 2,
    CMD_REQUEST = 0x50,
    CMD_REQUEST_SUCCESS = 0x51
};



#define SZ_SIZET sizeof(size_t)
#define SZ_CHAR sizeof(char)


struct update_entry {
    u_int32_t start;
    u_int32_t len;
    //size_t age;
    byte *newdata;


};

struct init_arg {
    struct blob_t * blob;
    struct view * view;


};
void updater_init(struct view * view);
void *start(void *arg);
void fromMain(short events);
void fromPaula(short events);
void getUpdates_fromPaula();

void getUpdates_fromPaula_insert();
void setup_sock();

void updatefromBlob(struct blob_t * blob, size_t pos, size_t len);

void sendCommandToUpdater(char * cmd, char * resultbuf);
void requestCommandPaula();






#endif //HYXWIP_UPDATER_H

