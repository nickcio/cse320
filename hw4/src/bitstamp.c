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
#include "argo.h"
#include "store.h"

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
            //exit(EXIT_FAILURE);
        }
        if(fcntl(fd[0],F_SETFL,O_NONBLOCK | O_ASYNC) == 1) {
            perror("fcntl");
            //exit(EXIT_FAILURE);
        }
        
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
    if(wp->serial > 4) {
        char *start = txt;
        while(*start != 'S' && *start != '\0') start++;
        txt = start;
    }
    if(wp->trace) {
        struct timespec thetime;
        clock_gettime(CLOCK_REALTIME,&thetime);
        fprintf(stderr,"[%ld.%.6ld][%-10s][%2d][%5d]: %s\n",thetime.tv_sec,thetime.tv_nsec/1000,wp->wtype->name,wp->ifd,wp->serial,txt);
    }
    if(wp->serial > 4) {
        char *start = txt;
        while(*start != '{' && *start != '\0') start++;
        txt = start;

        FILE *fp;
        size_t bsize = 0;
        char *buffer;
        if((fp = open_memstream(&buffer,&bsize)) == NULL) {
            perror("stream");
        }
        fprintf(fp,"%s",txt);
        fflush(fp);
        buffer[bsize-1]='\0';
        ARGO_VALUE *jsonf = argo_read_value(fp);
        fclose(fp);
        free(buffer);

        ARGO_VALUE *data = argo_value_get_member(jsonf,"data");
        ARGO_VALUE *argpri = argo_value_get_member(data,"price");
        FILE *fp2;
        size_t bsize2 = 0;
        char *buffer2;
        if((fp2 = open_memstream(&buffer2,&bsize2)) == NULL) {
            perror("stream");
        }
        fprintf(fp2,"%s:%s:price",wp->wtype->name,wp->args[0]);
        double pri;
        argo_value_get_double(argpri,&pri);
        struct store_value price;
        price.type = STORE_DOUBLE_TYPE;
        price.content.double_value = pri;
        fclose(fp2);
        store_put(buffer2,&price);
        free(buffer2);

        ARGO_VALUE *argamt = argo_value_get_member(data,"amount");
        FILE *fp3;
        size_t bsize3 = 0;
        char *buffer3;
        if((fp3 = open_memstream(&buffer3,&bsize3)) == NULL) {
            perror("stream");
        }
        fprintf(fp3,"%s:%s:amount",wp->wtype->name,wp->args[0]);
        double amt;
        argo_value_get_double(argamt,&amt);
        fclose(fp3);
        struct store_value amount;
        amount.type = STORE_DOUBLE_TYPE;
        amount.content.double_value = amt;
        store_put(buffer3,&amount);
        free(buffer3);

        FILE *fp4;
        size_t bsize4 = 0;
        char *buffer4;
        if((fp4 = open_memstream(&buffer4,&bsize4)) == NULL) {
            perror("stream");
        }
        fprintf(fp4,"%s:%s:volume",wp->wtype->name,wp->args[0]);
        fclose(fp4);
        struct store_value *volume = store_get(buffer4);
        if(volume == NULL) {
            struct store_value new;
            new.type = STORE_DOUBLE_TYPE;
            new.content.double_value = 0.0;
            volume = &new;
        }
        volume->content.double_value+=amt;
        store_put(buffer4,volume);
        free(buffer4);


    }
    return EXIT_SUCCESS;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    wp->trace = enable;
    return EXIT_SUCCESS;
}