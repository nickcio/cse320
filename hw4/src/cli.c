#include <stdlib.h>
#include <unistd.h>
#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
#include "thewatcher.h"
#include <regex.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

extern int idcount;
extern int pipedinput;
extern struct {
    int length;
    WATCHER *first;
} watcher_list;

WATCHER *cli_watcher_start(WATCHER_TYPE *type, char *args[]) {
    WATCHER this = {
        .id = idcount++,
        .wtype = type,
        .pid = -1,
        .ifd = STDIN_FILENO,
        .ofd = STDOUT_FILENO,
        .trace = 0,
        .serial = 0,
        .args = NULL
    };
    WATCHER *cliw = malloc(sizeof(WATCHER));
    *cliw = this;
    cliw->next = cliw;
    cliw->prev = cliw;
    add_watcher(cliw);
    return cliw;
}

int cli_watcher_stop(WATCHER *wp) {
    cli_watcher_send(wp,"???\n");
    return EXIT_SUCCESS;
}

int cli_watcher_send(WATCHER *wp, void *arg) {
    char *carg = arg;
    dprintf(wp->ofd,"%s",carg);
    fflush(stdout);
    return EXIT_SUCCESS;
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    char *buffer = txt;
    if(wp->trace) {
        struct timespec thetime;
        clock_gettime(CLOCK_REALTIME,&thetime);
        fprintf(stderr,"[%ld.%.6ld][%-10s][%2d][%5d]: %s\n",thetime.tv_sec,thetime.tv_nsec/1000,wp->wtype->name,wp->ifd,wp->serial,txt);
    }
    regex_t regwatch;
    int regerr = regcomp(&regwatch,"^watchers(\\s)*\n$",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regstart;
    regerr = regcomp(&regstart,"^start(\\s)+((\\S+)(\\s)+)*(\\S+)(\\s)*\n$",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regstop;
    regerr = regcomp(&regstop,"^stop(\\s)+[0-9]+(\\s)*\n$",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regtrace;
    regerr = regcomp(&regtrace,"^trace(\\s)+[0-9]+(\\s)*\n$",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t reguntrace;
    regerr = regcomp(&reguntrace,"^untrace(\\s)+[0-9]+(\\s)*\n$",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regshow;
    regerr = regcomp(&regshow,"^show(\\s)+[0-9]+(\\s)*\n$",REG_EXTENDED);
    if(regerr) sigint_handler();
    
    int val = -2;
    if(((val = strcmp("quit\n\0",buffer)) == 0) || buffer[0] == EOF) {
        sigint_handler(); //Gracefully quits 
    }
    else if((val = regexec(&regwatch,buffer,0,NULL,0)) == 0) {
        if(!pipedinput) cli_watcher_send(wp,"ticker> ");
        WATCHER *curr = watcher_list.first;
        while(curr != NULL) {
            dprintf(wp->ofd,"%d\t%s(%d,%d,%d) ",curr->id,curr->wtype->name,curr->pid,curr->ifd,curr->ofd);
            char **argv = curr->wtype->argv;
            if(argv != NULL) {
                int i = 0;
                char *carg = argv[0];
                while(carg != NULL) {
                    dprintf(wp->ofd,"%s ",carg);
                    i++;
                    carg = argv[i];
                }
            }
            char **args = curr->args;
            if(args != NULL) {
                dprintf(wp->ofd,"[");
                int i = 0;
                char *carg = args[0];
                while(carg != NULL) {
                    dprintf(wp->ofd,"%s",carg);
                    i++;
                    carg = args[i];
                    if(carg != NULL) dprintf(wp->ofd," ");
                }
                dprintf(wp->ofd,"]");
            }
            dprintf(wp->ofd,"\n");
            curr = curr->next;
        }
        fflush(stdout);
    }
    else if((val = regexec(&regstart,buffer,0,NULL,0)) == 0) { //takes several args
        char **args = parse_args(buffer+5);
        int wat = -2;
        int wtype = 0;
        int found = 0;
        while(watcher_types[wtype].name != NULL) {
            if((wat = strcmp(args[0],watcher_types[wtype].name))==0) {
                fprintf(stderr,"FOUND TYPE! %d\n",wtype);
                found = 1;
                break;
            }
            wtype++;
        }
        if(found) {
            if(wtype == CLI_WATCHER_TYPE) cli_watcher_send(wp,"???\n");
            else{
                char **inargs = args+1;
                if(*inargs == NULL) inargs = NULL;
                watcher_types[wtype].start(&watcher_types[wtype],inargs);
            }
        }
        else cli_watcher_send(wp,"???\n");
    }
    else if((val = regexec(&regstop,buffer,0,NULL,0)) == 0) { //takes 1 arg
        char* arg = parse_args(buffer+5)[0];
        int idnum = 0;
        int valid = 1;
        while(*arg != '\0') {
            if(!isdigit(*arg)) {
                valid = 0;
                break;
            }
            idnum*=10;
            idnum+=(*arg)-48;
            arg++;
        }
        if(!valid) cli_watcher_send(wp,"???\n");
        else{
            WATCHER *this = find_watcher(idnum);
            if(this != NULL) this->wtype->stop(this);
            else cli_watcher_send(wp,"???\n");
        }
    }
    else if((val = regexec(&regtrace,buffer,0,NULL,0)) == 0) { //takes 1 arg
        //get ID
        char* arg = parse_args(buffer+5)[0];
        int idnum = 0;
        int valid = 1;
        while(*arg != '\0') {
            if(!isdigit(*arg)) {
                valid = 0;
                break;
            }
            idnum*=10;
            idnum+=(*arg)-48;
            arg++;
        }
        if(!valid) cli_watcher_send(wp,"???\n");
        else{
            WATCHER *this = find_watcher(idnum);
            if(this != NULL) this->wtype->trace(this,1);
            else cli_watcher_send(wp,"???\n");
        }
    }
    else if((val = regexec(&reguntrace,buffer,0,NULL,0)) == 0) { //takes 1 arg
        //get ID
        char* arg = parse_args(buffer+7)[0];
        int idnum = 0;
        int valid = 1;
        while(*arg != '\0') {
            if(!isdigit(*arg)) {
                valid = 0;
                break;
            }
            idnum*=10;
            idnum+=(*arg)-48;
            arg++;
        }
        if(!valid) cli_watcher_send(wp,"???\n");
        else{
            WATCHER *this = find_watcher(idnum);
            if(this != NULL) this->wtype->trace(this,0);
            else cli_watcher_send(wp,"???\n");
        }
    }
    else if((val = regexec(&regshow,buffer,0,NULL,0)) == 0) { //takes 1 arg
        fprintf(stderr,"SHOW!\n");
    }
    else if(pipedinput) {
        cli_watcher_send(wp,"???\n");
    }
    if(!pipedinput) {
        pipedinput = 1;
        if(strlen(buffer) != 0) {
            cli_watcher_send(wp,"ticker> ");
            sigint_handler();
        }
    }
    wp->serial++;
    return EXIT_SUCCESS;
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    wp->trace = enable;
    return EXIT_SUCCESS;
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
        curr->wtype->stop(curr);
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