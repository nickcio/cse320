#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "ticker.h"
#include "bitstamp.h"
#include "debug.h"
#include <fcntl.h>
#include "thewatcher.h"

extern int idcount;
extern struct {
    int length;
    WATCHER *first;
} watcher_list;

WATCHER *bitstamp_watcher_start(WATCHER_TYPE *type, char *args[]) {
    int fd[2];
    pipe(fd);
    int fd2[2];
    pipe(fd2);
    int pid = -1;
    if((pid = fork()) != 0) {
        WATCHER this = {
            .id = idcount++,
            .wtype = type,
            .pid = pid,
            .ifd = fd[0],
            .ofd = fd2[1],
            .trace = 0,
            .serial = 0,
            .args = args
        };
        WATCHER *bw = malloc(sizeof(WATCHER));
        *bw = this;
        bw->next = bw;
        bw->prev = bw;
        add_watcher(bw);
        close(fd[1]);
        close(fd2[0]);

        return bw;
    }
    else{
        sleep(1);
        dup2(fd[1],STDIN_FILENO);
        dup2(fd2[0],STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        close(fd2[0]);
        close(fd2[1]);

        if(fcntl(STDIN_FILENO,F_SETOWN,getpid()) == 1) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }
        if(fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK | O_ASYNC) == 1) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }

    }
    return NULL;
}

int bitstamp_watcher_stop(WATCHER *wp) {
    if(wp == NULL) return EXIT_FAILURE;
    del_watcher(wp->id);
    char **args = wp->args;
    if(args != NULL) {
        char **start = args;
        while(*args != NULL){
            free(*args);
            args++;
        }
        free(start[-1]);
        free(start-1);
    }
    return EXIT_SUCCESS;
}

int bitstamp_watcher_send(WATCHER *wp, void *arg) {
    // TO BE IMPLEMENTED
    return EXIT_SUCCESS;
}

int bitstamp_watcher_recv(WATCHER *wp, char *txt) {
    // TO BE IMPLEMENTED
    return EXIT_SUCCESS;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    wp->trace = enable;
    return EXIT_SUCCESS;
}