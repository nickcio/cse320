#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "debug.h"
#include "ticker.h"
#include <regex.h>
#include "thewatcher.h"
#include <ctype.h>

volatile int sigflag = 0;
volatile int pipedinput = 0;
volatile int donepiping = 0;
volatile int inputpipe = -1;
int idcount = 0;

struct {
    int length;
    WATCHER *first;
} watcher_list;

WATCHER *cli = NULL;

char **parse_args(char *txt) {
    if(txt == NULL) return NULL;
    //fprintf(stderr,"Txt: %s\n",txt);
    char *start = txt;
    int argcount = 0;
    int lastspace = 1;
    while(isspace(*start) && *start != '\0') start++;
    //fprintf(stderr,"Start: %s\n",start);
    char* pc = start;
    while(*pc != '\0') {
        if(!isspace(*pc) && lastspace) argcount++;
        if(isspace(*pc)) lastspace = 1;
        else lastspace = 0;
        pc++;
    }
    char **args = malloc(sizeof(char *)*(argcount + 1));
    int c = 0;
    char *pp = start;
    while(c < argcount) {
        FILE *fp;
        size_t bsize = 0;
        char *buffer;
        if((fp = open_memstream(&buffer,&bsize)) == NULL) {
            perror("stream");
            exit(EXIT_FAILURE);
        }
        while(!isspace(*pp) && *pp != '\0') {
            fprintf(fp,"%c",*pp);
            fflush(fp);
            pp++;
        }
        fclose(fp);
        args[c] = buffer;
        //fprintf(stderr,"Arg %d: %s\n",c,buffer);
        c++;
        while(isspace(*pp) && *pp != '\0') {
            pp++;
            if(*pp == '\0') break;
        }
    }
    args[argcount] = NULL;
    return args;
}

void sigchld_handler() {
    waitpid(-1,NULL,WNOHANG);
}

void genio_handler() {
    FILE *fp;
    size_t bsize = 0;
    char *buffer;
    if((fp = open_memstream(&buffer,&bsize)) == NULL) {
        perror("stream");
        exit(EXIT_FAILURE);
    }
    WATCHER *curr = watcher_list.first;
    while(curr != NULL) {
        char temp[4096] = {'\0'};
        int valid = read(curr->ifd,temp,4096);
        if(valid != -1) {
            //fprintf(stderr,"This file: %d Input: %d\n",curr->ifd,valid);
            fprintf(fp,"%s",temp);
            fflush(fp);
            sigio_handler_ext(fp,buffer,&bsize,valid,curr);
            fclose(fp);
            free(buffer);
            break;
        }
        curr = curr->next;
    }
}

void sigint_handler() {
    WATCHER *start = watcher_list.first->next;
    while(start != NULL) {
        waitpid(start->pid,NULL,WNOHANG);
        start->wtype->stop(start);
        WATCHER *last = start;
        start = start->next;
        free(last);
    }
    free(watcher_list.first);
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

void sigio_handler_ext(FILE *fp, char *buffer,size_t *bsize,int end,WATCHER *wp) {
    debug("Signo");
    //fprintf(stderr,"BUFFAH: %s %d\n",buffer,*bsize);
    int total = end;
    while(buffer[total-1] != '\n' && end != -1 && (strcmp(wp->wtype->name,"CLI") == 0)) {
        char temp[4096] = {'\0'};
        int nex = read(STDIN_FILENO,temp,4096);
        if(nex > 0) {
            total += nex;
            fprintf(fp,"%s",temp);
            fflush(fp);
        }
        else if(nex == 0) {
            if(bsize > 0) free(buffer);
            sigint_handler();
        }
    }

    
    char *bp = buffer;
    while(*bp != '\0') {
        char *zp = bp;
        int size = 0;
        while(*zp != '\n' && *zp != '\0') {
            zp++;
            size++;
        }
        if(*zp == '\0') break;
        char *seq = calloc(size+2,sizeof(char));
        memcpy(seq,bp,size+1);
        seq[size+1] = '\0';
        //fprintf(stderr,"BRUH! %s %s %d\n",seq,wp->wtype->name,end);
        wp->wtype->recv(wp,seq);
        free(seq);
        bp = zp+1;
    }
    sigflag = 0;
    pipedinput = 1;
}

void sigio_handler() {
    debug("Signo");
    
    FILE *fp;
    size_t bsize = 0;
    char *buffer;
    if((fp = open_memstream(&buffer,&bsize)) == NULL) {
        perror("stream");
        exit(EXIT_FAILURE);
    }

    char temp[1024] = {'\0'};
    int end = read(STDIN_FILENO,temp,1024);
    if(end == 0) {
        free(buffer);
        fclose(fp);
        sigint_handler();
    }
    fprintf(fp,"%s",temp);
    fflush(fp);
    if(bsize == 0) donepiping = 1;
    int total = end;
    while(buffer[total-1] != '\n' && end != -1) {
        memset(temp,0,1024);
        int nex = read(STDIN_FILENO,temp,1024);
        if(nex > 0) {
            total += nex;
            fprintf(fp,"%s",temp);
            fflush(fp);
        }
        else if(nex == 0) {
            free(buffer);
            fclose(fp);
            sigint_handler();
        }
    }

    
    char *bp = buffer;
    while(*bp != '\0') {
        char *zp = bp;
        int size = 0;
        while(*zp != '\n' && *zp != '\0') {
            zp++;
            size++;
        }
        char *seq = calloc(size+2,sizeof(char));
        if(*zp == '\0') {
            free(seq);
            break;
        }
        memcpy(seq,bp,size+1);
        seq[size+1] = '\0';
        watcher_types[CLI_WATCHER_TYPE].recv(cli,seq);
        free(seq);
        bp = zp+1;
    }
    free(buffer);
    if(!donepiping) sigint_handler();
    sigflag = 0;
    pipedinput = 1;
}



void handler(int signo,siginfo_t *siginfo,void* context) {
    switch(signo) {
        case SIGIO:
            sigflag = SIGIO;
            inputpipe = siginfo->si_fd;
            break;
        case SIGINT:
            sigint_handler();
            break;
        case SIGCHLD:
            sigflag = SIGCHLD;
            break;
        default:
            sigflag = 0;
            break;
    }
}

int ticker(void) {
    cli = watcher_types[CLI_WATCHER_TYPE].start(&watcher_types[CLI_WATCHER_TYPE],NULL);
    sigflag = 0;
    //struct sigaction newaction = {0};
    struct sigaction asigio;
    memset(&asigio, 0x00, sizeof(asigio));
    sigemptyset(&asigio.sa_mask);
    asigio.sa_sigaction = handler;
    asigio.sa_flags = SA_SIGINFO;

    struct sigaction asigint;
    memset(&asigint, 0x00, sizeof(asigint));
    sigemptyset(&asigint.sa_mask);
    asigint.sa_sigaction = handler;
    asigint.sa_flags = SA_SIGINFO;

    struct sigaction asigchld;
    memset(&asigchld, 0x00, sizeof(asigchld));
    sigemptyset(&asigchld.sa_mask);
    asigchld.sa_sigaction = handler;
    asigchld.sa_flags = SA_SIGINFO;

    sigaction(SIGIO,&asigio,NULL);
    sigaction(SIGINT,&asigint,NULL);
    sigaction(SIGCHLD,&asigchld,NULL);
    
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask,SIGINT);
    sigdelset(&mask,SIGCHLD);
    sigdelset(&mask,SIGIO);

    if(fcntl(STDIN_FILENO,F_SETOWN,getpid()) == 1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
    if(fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK | O_ASYNC) == 1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
    sigio_handler(); //handle piped input
    donepiping = 1;
    cli->wtype->send(cli,"ticker> ");
    sigprocmask(SIG_UNBLOCK,&mask,NULL);
    while(1) {
        if(sigflag == SIGIO) {
            genio_handler();
        }
        if(sigflag == SIGCHLD) sigchld_handler();
        fflush(stdout);
        sigsuspend(&mask);
    }


    return 0;
}

int add_watcher(WATCHER *watcher) {
    if(watcher_list.length == 0) {
        watcher_list.first = watcher;
        watcher->next = NULL;
        watcher->prev = NULL;
    }
    else {
        WATCHER *curr = watcher_list.first;
        while(curr->next != NULL) curr = curr->next;
        curr->next = watcher;
        watcher->prev = curr;
        watcher->next = NULL;
    }
    watcher_list.length+=1;
    return 0;
}

int del_watcher(int id) {
    if(watcher_list.length <= 0) {
        watcher_list.length = 0;
        return -1;
    }
    else {
        WATCHER *curr = find_watcher(id);
        if(curr==NULL) return -1;
        if(curr->prev != NULL) curr->prev->next = curr->next;
        if(curr->next != NULL) curr->next->prev = curr->prev;
    }
    watcher_list.length-=1;
    if(watcher_list.length==0)watcher_list.first = NULL;
    return 0;
}

WATCHER *find_watcher(int id) {
    WATCHER *curr = watcher_list.first;
    while(curr != NULL && curr->id != id) curr = curr->next;
    return curr;
}