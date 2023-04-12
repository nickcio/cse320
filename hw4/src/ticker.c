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
    fprintf(stderr,"SIGCHLD!\n");
}

void sigint_handler() {
    debug("Done");
    WATCHER *start = watcher_list.first->next;
    while(start != NULL) {
        waitpid(start->pid,NULL,WNOHANG);
        start->wtype->stop(start);
        start = start->next;
    }
    fflush(stdout);
    exit(EXIT_SUCCESS);
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
        if(bsize > 0) free(buffer);
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
        watcher_types[CLI_WATCHER_TYPE].recv(cli,seq);
        free(seq);
        bp = zp+1;
    }
    free(buffer);
    if(!donepiping) sigint_handler();
    sigflag = 0;
    pipedinput = 1;
}

void handler(int signo) {
    switch(signo) {
        case SIGINT:
            sigint_handler();
            break;
        case SIGCHLD:
            sigflag = SIGCHLD;
            break;
        case SIGIO:
            sigflag = SIGIO;
            break;
        default:
            sigflag = 0;
            break;
    }
}

int ticker(void) {
    cli = watcher_types[CLI_WATCHER_TYPE].start(&watcher_types[CLI_WATCHER_TYPE],NULL);
    sigflag = 0;
    struct sigaction newaction = {0};
    newaction.sa_handler = handler;
    sigemptyset(&newaction.sa_mask);
    newaction.sa_flags = 0;
    sigaction(SIGINT,&newaction,NULL);
    sigaction(SIGIO,&newaction,NULL);
    sigaction(SIGCHLD,&newaction,NULL);
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
        if(sigflag == SIGIO) sigio_handler();
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