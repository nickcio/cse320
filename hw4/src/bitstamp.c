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
#include <signal.h>
#include <sys/wait.h>

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
        bw->self = bw;
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
        free(start);
    }
    kill(wp->pid,SIGTERM);
    waitpid(wp->pid,NULL,WNOHANG);
    free(wp);
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
        if(jsonf != NULL) {
            ARGO_VALUE *data = argo_value_get_member(jsonf,"data");
            if(data != NULL) {
                ARGO_VALUE *event = argo_value_get_member(jsonf,"event");
                char *event_trade = NULL;
                if(event != NULL) {
                    event_trade = argo_value_get_chars(event);
                }
                if(event_trade != NULL && strcmp(event_trade,"trade") == 0) {
                    ARGO_VALUE *argpri = argo_value_get_member(data,"price");
                    if(argpri != NULL) {
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
                    }

                    ARGO_VALUE *argamt = argo_value_get_member(data,"amount");
                    if(argamt != NULL) {
                        double amt;
                        argo_value_get_double(argamt,&amt);
                        //fclose(fp3);
                        //struct store_value amount;
                        //amount.type = STORE_DOUBLE_TYPE;
                        //amount.content.double_value = amt;
                        //store_put(buffer3,&amount);
                        //free(buffer3);

                        FILE *fp4;
                        size_t bsize4 = 0;
                        char *buffer4;
                        if((fp4 = open_memstream(&buffer4,&bsize4)) == NULL) {
                            perror("stream");
                        }
                        fprintf(fp4,"%s:%s:volume",wp->wtype->name,wp->args[0]);
                        fclose(fp4);
                        struct store_value *volume = store_get(buffer4);
                        int wasnull = 0;
                        if(volume == NULL) {
                            wasnull = 1;
                            struct store_value new;
                            new.type = STORE_DOUBLE_TYPE;
                            new.content.double_value = 0.0;
                            volume = &new;
                        }
                        volume->content.double_value+=amt;
                        store_put(buffer4,volume);
                        if(!wasnull) store_free_value(volume);
                        free(buffer4);
                    }
                }
                if(event_trade != NULL) free(event_trade);
            }
            argo_free_value(jsonf);
            jsonf = NULL;
        }
        if(jsonf != NULL) argo_free_value(jsonf);

    }
    return EXIT_SUCCESS;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    wp->trace = enable;
    return EXIT_SUCCESS;
}