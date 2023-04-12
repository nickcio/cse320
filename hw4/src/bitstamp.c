#include <stdlib.h>
#include <stdio.h>

#include "ticker.h"
#include "bitstamp.h"
#include "debug.h"
#include "thewatcher.h"

extern int idcount;
extern struct {
    int length;
    WATCHER *first;
} watcher_list;

WATCHER *bitstamp_watcher_start(WATCHER_TYPE *type, char *args[]) {
    fprintf(stderr,"BITSTAMP MAKE!\n");
    WATCHER this = {
        .id = idcount++,
        .wtype = type,
        .pid = -1,
        .ifd = 0,
        .ofd = 1,
        .trace = 0,
        .serial = 0,
        .args = args
    };
    WATCHER *bw = malloc(sizeof(WATCHER));
    *bw = this;
    bw->next = bw;
    bw->prev = bw;
    add_watcher(bw);
    return bw;
}

int bitstamp_watcher_stop(WATCHER *wp) {
    if(wp == NULL) return EXIT_FAILURE;
    del_watcher(wp->id);
    char **args = wp->args;
    if(args != NULL) {
        while(*args != NULL){
            free(*args);
            args++;
        }
        free(args);
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