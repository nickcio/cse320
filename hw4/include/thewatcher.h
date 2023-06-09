#define WATCHER_INCLUDED 1
#include "ticker.h"
typedef struct watcher {
    int id; //watcher id
    WATCHER_TYPE *wtype; //watcher type
    int pid; //process id of child process executing this watcher
    int ifd; //file descriptor of input
    int ofd; //file descriptor of output
    WATCHER *next;
    WATCHER *prev;
    int trace;
    int serial;
    char **args; //additional args
    WATCHER *self;
} WATCHER;

char **parse_args(char *txt,int skip); //parses args, allocates data

int add_watcher(WATCHER *watcher); //Add by pointer

int del_watcher(int id); //Delete by id

WATCHER *find_watcher(int id);

void genio_handler();

int sigio_handler_ext(FILE *fp, char *buffer,size_t *bsize,int end,WATCHER *wp);

void sigint_handler();