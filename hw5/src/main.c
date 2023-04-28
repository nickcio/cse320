#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>

#include "csapp.h"
#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);
void *thread(void *vargp);

static void sighup_handler();
/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    if(argc != 3) {
        exit(EXIT_FAILURE);
    }
    int opt;
    int gotp = 0;
    char *port = NULL;
    while((opt = getopt(argc,argv,"p:")) != -1) {
        switch(opt){
            case 'p':
                //int pnum = 0;
                char *op = optarg;
                while(*op != '\0') {
                    if(!isdigit(*op)) {
                        debug("Invalid port.");
                        exit(EXIT_FAILURE);
                    }
                    op++;
                }
                debug("Valid port. optarg %s",optarg);
                gotp = 1;
                port = optarg;
                break;
            default:
                debug("Invalid input.");
                break;
        }
    }
    if(!gotp) exit(EXIT_FAILURE);
    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    struct sigaction asighup;
    memset(&asighup, 0x00, sizeof(asighup));
    sigemptyset(&asighup.sa_mask);
    asighup.sa_sigaction = sighup_handler;
    asighup.sa_flags = SA_SIGINFO;

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port);
    if(listenfd == -1 || listenfd == -2) {
        debug("Server not made.");
        exit(EXIT_FAILURE);
    }
    else debug("Server made successfully.");
    while(1) {
        debug("X1");
        clientlen=sizeof(struct sockaddr_storage);
        debug("X2");
        connfdp = Calloc(1,sizeof(int));
        debug("X3");
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        if(*connfdp < 0) {
            debug("X5");
        }
        debug("X4");
        Pthread_create(&tid, NULL, jeux_client_service, connfdp);
    }

    //fprintf(stderr, "You have to finish implementing main() before the Jeux server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}

void sighup_handler() {
    debug("Sighup BRO");
    terminate(EXIT_SUCCESS);
}