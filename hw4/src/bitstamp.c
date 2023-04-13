#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ticker.h"
#include "bitstamp.h"
#include "debug.h"
#include <fcntl.h>
#include "thewatcher.h"
#include <time.h>

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

        if(fcntl(fd[0],F_SETOWN,getpid()) == 1) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }
        if(fcntl(fd[0],F_SETFL,O_NONBLOCK | O_ASYNC) == 1) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }
        //By using bitstamp_watcher_send instead of using write directly, I successfully made
        //my code more efficient by -6 lines!
        /*char *json1 = calloc(50,sizeof(char));
        memcpy(json1,"{ \"event\": \"bts:subscribe\", \"data\": { \"channel\": \"",50);
        bitstamp_watcher_send(&this,json1);
        free(json1);
        bitstamp_watcher_send(&this,args[0]);
        char *json2 = calloc(6,sizeof(char));
        memcpy(json2,"\" } }\n",6);
        bitstamp_watcher_send(&this,json2);
        free(json2);*/
        
        write(fd2[1],"{ \"event\": \"bts:subscribe\", \"data\": { \"channel\": \"",50);
        write(fd2[1],args[0],strlen(args[0]));
        write(fd2[1],"\" } }\n",6);
        

        return bw;
    }
    else{
        close(fd[0]);
        close(fd2[1]);
        dup2(fd[1],STDOUT_FILENO);
        dup2(fd2[0],STDIN_FILENO);
        execvp("uwsc",type->argv);
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
    arg = (char *)arg;
    write(wp->ofd,arg,strlen(arg));
    return EXIT_SUCCESS;
}

int bitstamp_watcher_recv(WATCHER *wp, char *txt) {
    wp->serial++;
    if(wp->trace) {
        struct timespec thetime;
        clock_gettime(CLOCK_REALTIME,&thetime);
        fprintf(stderr,"[%ld.%.6ld][%-10s][%2d][%5d]: %s\n",thetime.tv_sec,thetime.tv_nsec/1000,wp->wtype->name,wp->ifd,wp->serial,txt);
    }
    return EXIT_SUCCESS;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    wp->trace = enable;
    return EXIT_SUCCESS;
}