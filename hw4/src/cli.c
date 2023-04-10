#include <stdlib.h>
#include <unistd.h>
#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
#include "thewatcher.h"
#include <regex.h>
#include <string.h>

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
        .args = args
    };
    WATCHER *cliw = malloc(sizeof(WATCHER));
    *cliw = this;
    cliw->next = cliw;
    cliw->prev = cliw;
    add_watcher(cliw);
    return cliw;
}

int cli_watcher_stop(WATCHER *wp) {
    fprintf(stdout,"???\n");
    return EXIT_SUCCESS;
}

int cli_watcher_send(WATCHER *wp, void *arg) {
    // TO BE IMPLEMENTED
    abort();
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    char *buffer = txt;
    regex_t regwatch;
    int regerr = regcomp(&regwatch,"watchers(\\s)*\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regstart;
    regerr = regcomp(&regstart,"start(\\s)+((\\S+)(\\s)+)*(\\S+)(\\s)*\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regstop;
    regerr = regcomp(&regstop,"stop(\\s)+[0-9]+(\\s)*\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regtrace;
    regerr = regcomp(&regtrace,"trace(\\s)+[0-9]+(\\s)*\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t reguntrace;
    regerr = regcomp(&reguntrace,"untrace(\\s)+[0-9]+(\\s)*\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    regex_t regshow;
    regerr = regcomp(&regshow,"show(\\s)+[0-9]+(\\s)*\n",REG_EXTENDED);
    if(regerr) sigint_handler();
    
    int val = -2;
    if(((val = strcmp("quit\n\0",buffer)) == 0) || buffer[0] == EOF) {
        sigint_handler(); //Gracefully quits 
    }
    else if((val = regexec(&regwatch,buffer,0,NULL,0)) == 0) {
        if(!pipedinput) fprintf(stdout,"ticker> ");
        WATCHER *curr = watcher_list.first;
        while(curr != NULL) {
            dprintf(wp->ofd,"%d\t%s(%d,%d,%d)\n",curr->id,curr->wtype->name,curr->pid,curr->ifd,curr->ofd);
            curr = curr->next;
        }
        fflush(stdout);
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
        dprintf(wp->ofd,"???\n");
        //fflush(stdout);
    }
    if(!pipedinput) {
        pipedinput = 1;
        if(strlen(buffer) != 0) {
            dprintf(wp->ofd,"ticker> ");
            sigint_handler();
        }
    }
    return EXIT_SUCCESS;
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    // TO BE IMPLEMENTED
    abort();
}
