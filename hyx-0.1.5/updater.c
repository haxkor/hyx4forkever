//#include "updater.h"
//#include <common.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <stdbool.h>

#include <assert.h>
#include "updater.h"
#include "view.h"

#define MAINFD 0
#define UPDATEFD 1



struct sockaddr_un to_paula;
struct blob blob;

int to_paula_fd;
extern char *socketname;

pthread_t updaterthread;
int updater_communicationfds[2];    // used to communicate changes in hyx to the updater
struct view *upd_viewPtr;
FILE *mylog;

pthread_mutex_t communication_mutex=PTHREAD_MUTEX_INITIALIZER;

int send_strict(int fd, const void * buf, size_t len, int flags){
    int result=send(fd,buf,len,flags);
    if (result != len) pdie("send");
    return result;
}

int recv_strict(int fd, void * buf, size_t len, int flags){
    int result= recv(fd,buf,len,flags);
    if (result != len) pdie("recv");
    return result;
}


void setup_sock() {
    memset(&to_paula, 0, sizeof(struct sockaddr_un));

    to_paula.sun_family = AF_UNIX;
    strcpy(to_paula.sun_path, socketname);
    to_paula_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    //sleep(1);

    //now connect to the paula server
    if (-1 == connect(to_paula_fd, (const struct sockaddr *) &to_paula, sizeof(struct sockaddr_un))) {
        printf("initial connect\n");
        perror("connect");
        exit(1);
    }

}

void getUpdates_fromPaula() {
    fprintf(mylog, "in getUpdates_paula\n");
    //get number of updates
    int ret, num_updates;
    ret = recv(to_paula_fd, &num_updates, 4, 0);
    if (ret != 4) pdie("recv");

    if (fflush(mylog)) printf("fflush log error");

    struct update_entry entries[num_updates];
    //fprintf(mylog, "num_up= %d\n", num_updates);

    for (int i = 0; i < num_updates; i++) {         //we get the updates
        //fprintf(mylog,"in getupdate get loop\n");
        struct update_entry * entry = &entries[i];

        recv_strict(to_paula_fd, &entry->start, 4, 0);

        recv_strict(to_paula_fd, &entry->len, 4, 0);

        size_t len = entry->len;
        entry->newdata = malloc(len);
        recv_strict(to_paula_fd, entry->newdata, len, 0);

    }

    // we write the updates
    if (0 != pthread_mutex_lock(&blob.mutex_data)) pdie("pthread_mutex_lock");
    for (int i = 0; i < num_updates; i++) {
        struct update_entry * entry = &entries[i];
        blob_replace(&blob, entry->start, entry->newdata, entry->len, false, false);
        view_dirty_fromto(upd_viewPtr, entry->start, entry->start + entry->len);
    }
    if (0 != pthread_mutex_unlock(&blob.mutex_data)) pdie("pthread_mutex_unlock");

    fprintf(mylog, "updates written, returning\n");
    fflush(mylog);

}

void fromPaula(short events) {
    //handle events here
    assert(events & POLLIN);
    if (events != POLLIN){
        if (events | POLLERR) {
            printf("connection to paula closed\n");
            exit(EXIT_FAILURE);
        } else {
            printf("unknown event happened in fromPaula(), event= %#x\n", events);
            exit(EXIT_FAILURE);
        }
    }

    fprintf(mylog, "fromPaula() has been called\n");
    char check = 0;

    recv_strict(to_paula_fd, &check, 1, 0);

    switch ((int) check) {
        case UPD_FROMPAULA:
            fprintf(mylog, "calling getUpdates\n");
            getUpdates_fromPaula();
            break;

        default:
            fprintf(mylog, "check = %d \n", (int) check);
            break;
    }

}

/* this is called by the functions changing the data of the blob. */
void updatefromBlob(struct blob * blob, size_t pos, size_t len) {
    int fd = updater_communicationfds[FDIND_VIEW];   //send it to the updater
    char type = UPD_FROMBLOB;     //to indicate an update
    fprintf(mylog, "in update from blob\n");


    pthread_mutex_lock(&communication_mutex);
    send_strict(fd, &type, 1, 0);
    send_strict(fd, &pos, SZ_SIZET, 0);

    send_strict(fd, &len, SZ_SIZET, 0);
    pthread_mutex_unlock(&communication_mutex); //as soon as i send it all in one go it should be unnecessary

}

size_t nextpos = 0;    /*this var is used to communicate bytechanges next to each other without sending pos twice */
bool nextpos_init = false;


/* the other thread wrote pos and len to the socket, we will send the bytes to paula */
void sendToPaula() {
    fprintf(mylog, "in sendToPaula");
    int fromfd = updater_communicationfds[FDIND_UPDATER];    //recv from main thread

    size_t pos, len;
    recv_strict(fromfd, &pos, SZ_SIZET, 0);
    recv_strict(fromfd, &len, SZ_SIZET, 0);

    /* if we just wrote to pos -1, we dont need to send the position again*/
    char check = UPD_FROMBLOB;
    if (!nextpos_init) {          //first time the method is called
        nextpos_init = true;
        nextpos = pos + len;
    } else {
        if (pos == nextpos) {
            check = UPD_FROMBLOBNEXT;
            nextpos += len;
        } else {
            nextpos = pos + len;
        }
    }


    // send it to paula
    if (false) {        // no truncation
        send_strict(to_paula_fd, &check, SZ_CHAR, 0);
        if (check != UPD_FROMBLOBNEXT) send_strict(to_paula_fd, &pos, SZ_SIZET, 0);
        send_strict(to_paula_fd, &len, SZ_SIZET, 0);
        send_strict(to_paula_fd, blob.data + pos, len, 0);
    } else {
        byte buf[SZ_CHAR + SZ_SIZET * 2 + len];
        memset(buf, 0, sizeof(buf));
        byte *bufptr = buf;
        int send_len = sizeof(buf) - SZ_SIZET;

        memcpy(bufptr, &check, SZ_CHAR);
        bufptr += SZ_CHAR;
        if (check != UPD_FROMBLOBNEXT) {
            memcpy(bufptr, &pos, SZ_SIZET);
            bufptr += SZ_SIZET;
            send_len = sizeof(buf);
        }
        memcpy(bufptr, &len, SZ_SIZET);
        bufptr += SZ_SIZET;
        memcpy(bufptr, blob.data + pos, len);

        send_strict(to_paula_fd, buf, send_len, 0);
    }


    fprintf(mylog, "sent stuff to paula, start= %d, len= %d\n", pos, len);
}

void requestCommandPaula(){
    int fd_main= updater_communicationfds[FDIND_UPDATER];
    char cmd[0x100];
    read(fd_main, cmd, 0x100); //FIXME make this a constant

    send_strict(to_paula_fd, cmd, 0x100,0); //send cmd to paula, get response
    recv_strict(to_paula_fd,cmd,0x100,0);

    write(fd_main, cmd, 0x100); //send it back to the main thread


}

void fromMain(short events) {
    assert(events | POLLIN);
    if (events != POLLIN){
        if (events | POLLERR)
            die("fromMain POLLERR");
        else die("unknown event in fromMain");
    }

    char check;
    int fd=updater_communicationfds[FDIND_UPDATER];

    if (1 != recv(fd,&check,1,0)) pdie("recv");

    switch ((int)check) {
        case UPD_FROMBLOB:
            sendToPaula();
            break;
        case CMD_REQUEST:
            requestCommandPaula();
            break;
        default:
            printf("in frommain, check= %d", check);
            exit(EXIT_FAILURE);
            die("fromMain encountered unknown check value");
    }


}

/* this is called by the main thread when the user wants to free a specific adress*/
void sendCmd(char * cmd, char * resultbuf){
    //send data to updater thread
    int fd=updater_communicationfds[MAINFD];
    byte buf[0x100];
    memset(buf,0,sizeof(buf));
    buf[0]= (byte)CMD_REQUEST;

    size_t cmdlen= min(strlen(cmd),sizeof(buf)-1); //avoid cheeky overflows
    memcpy(buf+1, cmd, cmdlen);

    write(fd,buf,sizeof(buf));  //write cmd to updater thread
    byte check[2];
    read(fd, (void *) check[0], 2); //wait for response of updater thread
    if (check[0] == CMD_REQUEST_SUCCESS){
        read(fd, resultbuf, check[1]);
    } else {
        read(fd, resultbuf,check[1]);
        strcpy(resultbuf,"error in transmission");
    }
}


#define XOR(A, B) (bool)(A)!=(bool)(B)

void *start(void *arg) {
    setup_sock();

    struct pollfd pollfd[2];
    memset(pollfd, 0, 2 * sizeof(struct pollfd));
    pollfd[IND_PAULA].fd = to_paula_fd;
    pollfd[IND_PAULA].events = POLLIN;

    pollfd[IND_VIEW].fd = updater_communicationfds[FDIND_UPDATER];
    pollfd[IND_VIEW].events = POLLIN;


    while (1) {
        fprintf(mylog, "in infinite loop\n");


        int ret = poll(pollfd, 2, 123000);
        fprintf(mylog, "poll returned %d\n", ret);
        fflush(mylog);

        short paula_revents = pollfd[IND_PAULA].revents;
        short view_revents = pollfd[IND_VIEW].revents;
        switch (ret) {
            case -1:
                pdie("poll");
            case 0:
                printf("poll timed out\n");
                exit(EXIT_FAILURE);
            case 1:
                assert(XOR(paula_revents, view_revents));   //if both events are set, somethings wrong
                fprintf(mylog, "case 1 in loop\n");
                if (paula_revents)
                    fromPaula(paula_revents);
                else {
                    pthread_mutex_lock(&communication_mutex);
                    fromMain(view_revents);
                    pthread_mutex_unlock(&communication_mutex);
                }
                break;
            case 2:     // we should update both realheap and our copy. For now, dismiss our copy
                fromMain(view_revents);
                fromPaula(paula_revents);
                break;
            default:
                printf("poll() returned %d, i did not expect that\n", ret);
                exit(EXIT_FAILURE);
        }


    }


}


void updater_init(struct view *view) {
    if (0 != socketpair(AF_UNIX, SOCK_DGRAM, 0, updater_communicationfds)) pdie("socketpair");
    mylog = fopen("logfile", "w");
    if (mylog < 0) pdie("fopen");
    fprintf(mylog, "init log");

    upd_viewPtr = view;

    pthread_create(&updaterthread, NULL, start, NULL);
}

