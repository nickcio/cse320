#include "thewatcher.h"

struct {
    int length;
    WATCHER *first;
} watcher_list;

int add_watcher(WATCHER *watcher); //Add by pointer

int del_watcher(int id); //Delete by id