#include "client_registry.h"
#include "jeux_globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "csapp.h"
#include "debug.h"

typedef struct client_registry {
    int clientnum;
    CLIENT *clients[MAX_CLIENTS];
    pthread_mutex_t lock;
    sem_t sema;
}CLIENT_REGISTRY;

CLIENT_REGISTRY *creg_init() {
    pthread_mutex_t lock;
    sem_t sema;
    pthread_mutex_init(&lock,NULL);
    CLIENT_REGISTRY *creg = calloc(1,sizeof(CLIENT_REGISTRY));
    creg->clients[0] = NULL;
    creg->clientnum = 0;
    creg->lock = lock;
    Sem_init(&sema,0,1);
    creg->sema = sema;
    return creg;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr) {
    pthread_mutex_lock(&cr->lock);
    if(cr != NULL) {
        if(cr->clients != NULL) {
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(cr->clients[i] != NULL) client_unref(cr->clients[i],"creg fini");
            }
        }
        pthread_mutex_unlock(&cr->lock);
        pthread_mutex_destroy(&cr->lock);
        free(cr);
    }
    else{
    pthread_mutex_unlock(&cr->lock);
    pthread_mutex_destroy(&cr->lock);
    }
}

/*
 * Register a client file descriptor.
 * If successful, returns a reference to the the newly registered CLIENT,
 * otherwise NULL.  The returned CLIENT has a reference count of two:
 * one count is for the reference held by the registry itself for as long
 * as the client remains connected and the other is for the reference
 * that is returned.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return a reference to the newly registered CLIENT, if registration
 * is successful, otherwise NULL.
 */
CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
    if(cr == NULL) return NULL;
    pthread_mutex_lock(&cr->lock);
    CLIENT *new = client_create(cr,fd);
    cr->clients[cr->clientnum] = new;
    //new = client_ref(new,"reg");
    if(cr->clientnum == 0) P(&cr->sema);
    cr->clientnum++;
    pthread_mutex_unlock(&cr->lock);
    return new;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {
    if(cr == NULL || client == NULL) return -1;
    debug("Creg unregister! %p %p",cr,client);
    pthread_mutex_lock(&cr->lock);
    debug("Creg unregister2! %p %p",cr,client);
    int i = 0;
    while(i < cr->clientnum) {
        debug("Cru! %d %d",i,cr->clientnum);
        if(cr->clients[i] == client) {
            break;
        }
    }
    debug("Creg unregister3! %p %p",cr,client);
    if(i == cr->clientnum) {
        debug("Client not found!");
        pthread_mutex_unlock(&cr->lock);
        return -1;
    }
    client_unref(client,"unreg");
    int j = i;
    debug("Here2 %d %d",j,cr->clientnum-1);
    while(j < cr->clientnum-1) {
        cr->clients[j] = cr->clients[j+1];
        j++;
    }
    debug("Here3");
    cr->clientnum--;
    cr->clients[cr->clientnum] = NULL;
    debug("Client unregistered! %d",cr->clientnum);
    if(cr->clientnum == 0) {
        debug("Creg empty!");
        V(&cr->sema);
    }
    pthread_mutex_unlock(&cr->lock);
    return 0;
}

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {
    pthread_mutex_lock(&cr->lock);
    debug("Clients cr %p",cr->clients);
    for(int i = 0; i < cr->clientnum; i++) {
        CLIENT *curr = cr->clients[i];
        debug("Current client %p",cr->clients[i]);
        char *currname = player_get_name(client_get_player(curr));
        debug("Current client name %s %p",currname,currname);
        if(currname != NULL && strcmp(currname,user) == 0) {
            pthread_mutex_unlock(&cr->lock);
            return curr;
        }
    }
    pthread_mutex_unlock(&cr->lock);
    return NULL;
}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of usernames is to be
 * obtained.
 * @return the list of usernames as a NULL-terminated array of strings.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
    pthread_mutex_lock(&cr->lock);
    PLAYER *players[cr->clientnum];
    debug("players: %p, clientnum: %d",players,cr->clientnum);
    int j = 0; //player num
    for(int i = 0; i < cr->clientnum; i++) {
        CLIENT *curr = cr->clients[i];
        PLAYER *play = client_get_player(curr);
        debug("client: %p player: %p",curr,play);
        if(play != NULL) {
            debug("playername: %s, j: %d",player_get_name(play),j);
            players[j] = play;
            j++;
        }
    }
    players[j] = NULL;
    PLAYER **players2 = calloc(j+1,sizeof(PLAYER *));
    memcpy(players2,players,(j)*sizeof(PLAYER *));
    //debug("player0: %s",player_get_name(players2[0]));
    pthread_mutex_unlock(&cr->lock);
    return players2;
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
    debug("P in CREG WAIT FOR EMPTY!");
    P(&cr->sema);
    debug("V in CREG WAIT FOR EMPTY!");
    V(&cr->sema);
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr) {
    debug("%p Creg shutdown all",cr);
    pthread_mutex_lock(&cr->lock);
    for(int i = 0; i < cr->clientnum; i++) {
        debug("%p Creg shutdown %d",cr,i);
        int fd = client_get_fd(cr->clients[i]);
        debug("%p Got fd %d",cr,fd);
        if(fd != -1) {
            debug("Shutting down client %d",fd);
            shutdown(fd,SHUT_RD);
        }
    }
    pthread_mutex_unlock(&cr->lock);
}