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

#define MAINFD 0
#define UPDATEFD 1

struct sockaddr_un to_paula;

void fromPaula();

int to_paula_fd;
char sockname[19] = "/tmp/paulasock";
pthread_mutex_t mutex_data = PTHREAD_MUTEX_INITIALIZER;

pthread_t updaterthread;
int updater_communicationfds[2];

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

void fromPaula(short events) {
    printf("fromPaula() has been called\n");
    char buf[32];
    recv(to_paula_fd, buf, 32, 0);
    printf(buf);
    sleep(4);

}

void fromMain(short events) {
    printf("fromMain() has been called\n");
    char buf[32];
    recv(updater_communicationfds[UPDATEFD], buf, 32, 0);
    puts(buf);

}

void pdie(char *s) {
    puts(s);
}

#define XOR(A,B) (bool)(A)!=(bool)(B)
void *start(void *arg) {
    setup_sock();

    struct pollfd pollfd[2];
    memset(pollfd, 0, 2 * sizeof(struct pollfd));
    pollfd[IND_PAULA].fd = to_paula_fd;
    pollfd[IND_PAULA].events = POLLIN;

    pollfd[IND_VIEW].fd = updater_communicationfds[FDIND_UPDATER];
    pollfd[IND_VIEW].events = POLLIN;


    while (1) {
        memset(pollfd, 0, 2 * sizeof(struct pollfd));

        int ret = poll(pollfd, 2, 123000);
        short paula_revents=pollfd[IND_PAULA].revents;
        short view_revents=pollfd[IND_VIEW].revents;
        switch (ret) {
            case -1:
                pdie("poll");
            case 0:
                printf("poll timed out\n");
                exit(EXIT_FAILURE);
            case 1:
                assert(XOR(paula_revents,view_revents));   //if both events are set, somethings wrong
                if (paula_revents)
                    fromPaula(paula_revents);
                else {
                    fromMain(view_revents);
                }
                break;
            case 2:     // we should update both realheap and our copy. For now, dismiss our copy
                fromPaula(pollfd[IND_PAULA].revents);
                break;
            default:
                printf("poll returned %d, i did not expect that\n", ret);
                exit(EXIT_FAILURE);
        }
    }


}


void updater_init(void *arg) {
    if (0 != socketpair(AF_UNIX, SOCK_DGRAM, 0, updater_communicationfds)) pdie("socketpair");


    pthread_create(&updaterthread, NULL, start, NULL);

}


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
