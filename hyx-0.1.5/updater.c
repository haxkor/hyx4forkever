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
char sockname[19] = "/tmp/paulasock";
pthread_mutex_t mutex_data = PTHREAD_MUTEX_INITIALIZER;

pthread_t updaterthread;
int updater_communicationfds[2];
struct view *upd_viewPtr;
FILE *mylog;

void setup_sock() {
    memset(&to_paula, 0, sizeof(struct sockaddr_un));

    to_paula.sun_family = AF_UNIX;
    strcpy(to_paula.sun_path, sockname);
    to_paula_fd = socket(AF_UNIX, SOCK_STREAM, 0);


    sleep(1);

    //now connect to the paula server
    if (-1 == connect(to_paula_fd, (const struct sockaddr *) &to_paula, sizeof(struct sockaddr_un))) {
        printf("initial connect");
        perror("connect");
        exit(1);
    }

}

void getUpdates_fromPaula() {
    fprintf(mylog, "in getUpdates_paula\n");
    //get number of updates
    int ret, num_updates = 69;
    ret = recv(to_paula_fd, &num_updates, 4, 0);
    if (ret != 4) pdie("recv");

    if (fflush(mylog)) printf("fflush log error");
    fprintf(mylog, "after fflush should work");

    struct update_entry entries[num_updates];
    //fprintf(mylog, "num_up= %d\n", num_updates);

    for (int i = 0; i < num_updates; i++) {         //we get the updates
        //fprintf(mylog,"in getupdate get loop\n");
        struct update_entry * entry = &entries[i];
        byte buf[0x100];

        ret = recv(to_paula_fd, &entry->start, 4, 0);
        fprintf(mylog, "first receive, ret=%d, start= %d \n", ret, entry->start);
        if (ret != 4) pdie("recv");

        fprintf(mylog, "before second receive\n");
        ret = recv(to_paula_fd, &entry->len, 4, 0);
        fprintf(mylog, "second receive, ret=%d, len= %d \n", ret, entry->len);
        if (ret != 4) pdie("recv");

        uint32_t len = entry->len;
        entry->newdata = malloc(len);
        ret = recv(to_paula_fd, entry->newdata, len, 0);
        fprintf(mylog, "received data. len=%d, ret=%d,   data=%llx", len, ret, *entry->newdata);
        if (ret != len) pdie("recv");

    }

    // we write the updates
    if (0 != pthread_mutex_lock(&blob.mutex_data)) pdie("pthread_mutex_lock");
    for (int i = 0; i < num_updates; i++) {
        struct update_entry * entry = &entries[i];
        blob_replace(&blob, entry->start, entry->newdata, entry->len, false);
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
            printf("unknown event happened in fromPaula(), event= %hd\n", events);
            exit(EXIT_FAILURE);
        }
    }

    fprintf(mylog, "fromPaula() has been called\n");
    char check;

    recv(to_paula_fd, &check, 1, 0);

    switch ((int) check) {
        case 1:
            fprintf(mylog, "calling getUpdates\n");
            getUpdates_fromPaula();
            break;

        default:
            fprintf(mylog, "check = %d \n", (int) check);
            break;

    }


}

void fromMain(short events) {
    printf("fromMain() has been called\n");
    char buf[32];
    recv(updater_communicationfds[UPDATEFD], buf, 32, 0);
    puts(buf);

}

/*
void pdie(char *s) {
    puts(s);
}           //*/

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
                    fromMain(view_revents);
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
    if (mylog < 0)
        pdie("fopen");

    fprintf(mylog, "init log");

    upd_viewPtr = view;

    pthread_create(&updaterthread, NULL, start, NULL);

}


/*
int main() {

    updater_init(NULL);
    puts("gonna schleep");
    char buf[10] = "AAAAAA";
    for (int i = 5; i < 60; i += 5) {
        sleep(i);
        buf[0] += i;
        send(updater_communicationfds[FDIND_VIEW], buf, 3, 0);
    }


    puts("bye");
}
 //*/
