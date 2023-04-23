typedef struct client {
    int fd;
    int ref;
    int login;
    char *name;
    struct client *next;
    struct client *prev;
}CLIENT;