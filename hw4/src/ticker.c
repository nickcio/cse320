#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "debug.h"
#include "ticker.h"

char *buffer;

void sigint_handler() {
    debug("Done");
    exit(EXIT_SUCCESS);
}

void sigio_handler() {
    debug("Signo");
    read(STDIN_FILENO,buffer,1024);
    fprintf(stderr,"Buffer: %s\n",buffer);
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
            sigio_handler();
            break;
        default:
            break;
    }
}

int ticker(void) {
    buffer = calloc(1024,sizeof(char));
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask,SIGINT);
    sigdelset(&mask,SIGCHLD);
    sigdelset(&mask,SIGIO);

    struct sigaction newaction = {0};
    newaction.sa_handler = handler;
    sigemptyset(&newaction.sa_mask);
    newaction.sa_flags = 0;
    sigaction(SIGINT,&newaction,NULL);
    sigaction(SIGIO,&newaction,NULL);
    sigaction(SIGCHLD,&newaction,NULL);

    if(fcntl(STDIN_FILENO,F_SETOWN,getpid()) == 1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
    if(fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK | O_ASYNC) == 1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    
    debug("Print process no: %d %d\n",fileno(stdin),STDIN_FILENO);
    sigprocmask(SIG_UNBLOCK,&mask,NULL);
    while(1) {
        sigsuspend(&mask);
    }


    return 0;
}