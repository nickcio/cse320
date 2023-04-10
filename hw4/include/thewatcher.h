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
    char **args; //additional args
} WATCHER;

