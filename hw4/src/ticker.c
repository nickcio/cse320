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

volatile int sigflag = 0;
volatile int pipedinput = 0;

void sigint_handler() {
    debug("Done");
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

    char temp[128] = {'\0'};
    int end = read(STDIN_FILENO,temp,128);
    if(end == 0) sigint_handler();
    fprintf(fp,"%s",temp);
    fflush(fp);

    regex_t regstart;
    int regerr = regcomp(&regstart,"start ((\\S+) )*(\\S+)\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regstop;
    regerr = regcomp(&regstop,"stop [0-9]+\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regtrace;
    regerr = regcomp(&regtrace,"trace [0-9]+\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t reguntrace;
    regerr = regcomp(&reguntrace,"untrace [0-9]+\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regshow;
    regerr = regcomp(&regshow,"show [0-9]+\n",REG_EXTENDED);
    if(regerr) sigint_handler();

    int val = -2;
    if(((val = strcmp("quit\n\0",buffer)) == 0) || buffer[0] == EOF) {
        sigint_handler(); //Gracefully quits 
    }
    else if((val = strncmp("watchers\n",buffer,9)) == 0) {
        fprintf(stderr,"WATCH!\n");
    }
    else if((val = regexec(&regstart,buffer,0,NULL,0)) == 0) { //takes several args
        fprintf(stderr,"START!\n");
    }
    else if((val = regexec(&regstop,buffer,0,NULL,0)) == 0) { //takes 1 arg
        fprintf(stderr,"STOP!\n");
    }
    else if((val = regexec(&regtrace,buffer,0,NULL,0)) == 0) { //takes 1 arg
        fprintf(stderr,"TRACE!\n");
    }
    else if((val = regexec(&reguntrace,buffer,0,NULL,0)) == 0) { //takes 1 arg
        fprintf(stderr,"UNTRACE!\n");
    }
    else if((val = regexec(&regshow,buffer,0,NULL,0)) == 0) { //takes 1 arg
        fprintf(stderr,"SHOW!\n");
    }
    else if(pipedinput) {
        fprintf(stdout,"???\n");
        fflush(stdout);
    }
    if(!pipedinput) {
        pipedinput = 1;
        sigint_handler();
    }
    free(buffer);
    sigflag = 0;
    pipedinput = 1;
}

void handler(int signo) {
    switch(signo) {
        case SIGINT:
            sigint_handler();
            break;
        case SIGCHLD:
            debug("Sigchld\n");
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
    sigio_handler();
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

    sigprocmask(SIG_UNBLOCK,&mask,NULL);
    //Initializing OVER!
    //sleep(0.01);
    while(1) {
        if(sigflag == SIGIO) sigio_handler();
        fprintf(stdout,"ticker> ");
        fflush(stdout);
        sigsuspend(&mask);
    }


    return 0;
}