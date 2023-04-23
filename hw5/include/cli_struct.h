typedef struct client {
    int fd;
    char *name;
    struct client *next;
    struct client *prev;
}CLIENT;