#include <stdlib.h>
#include <unistd.h>
#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
#include "thewatcher.h"

extern int idcount;

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

    return cliw;
}

int cli_watcher_stop(WATCHER *wp) {
    // TO BE IMPLEMENTED
    abort();
}

int cli_watcher_send(WATCHER *wp, void *arg) {
    // TO BE IMPLEMENTED
    abort();
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    // TO BE IMPLEMENTED
    abort();
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    // TO BE IMPLEMENTED
    abort();
}
