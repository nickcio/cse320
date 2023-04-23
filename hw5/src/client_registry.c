#include "cli_struct.h"
#include "client_registry.h"
#include "jeux_globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "csapp.h"

int clientnum;

typedef struct client_registry {
    CLIENT *first;
}CLIENT_REGISTRY;

pthread_mutex_t lock;
sem_t sema;
CLIENT_REGISTRY *creg_init() {
    pthread_mutex_init(&lock,NULL);
    clientnum = 0;
    CLIENT_REGISTRY *creg = malloc(sizeof(CLIENT_REGISTRY));
    CLIENT *new = malloc(sizeof(CLIENT));
    new->fd = -1;
    new->next = NULL;
    new->prev = NULL;
    Sem_init(&sema,0,1);
    creg->first = new;
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
    if(cr == NULL || fd < 0) return NULL;
    //pthread_mutex_lock(&lock);
    CLIENT *new = malloc(sizeof(CLIENT));
    new->fd = fd;
    new->next=NULL;
    CLIENT *curr = cr->first;
    while(curr->next != NULL) curr=curr->next;
    curr->next=new;
    new->prev=curr;
    new->name=NULL;
    clientnum++;
    if(clientnum == 1) P(&sema);
    pthread_mutex_unlock(&lock);
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
    //pthread_mutex_lock(&lock);
    close(client->fd);
    client->prev->next = client->next;
    client->next->prev = client->prev;
    free(client);
    clientnum--;
    if(clientnum == 0) V(&sema);
    pthread_mutex_unlock(&lock);
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
    CLIENT *curr = cr->first;
    while(curr != NULL) {
        if(strcmp(curr->name,user) == 0) return curr;
        curr = curr->next;
    }
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
    return NULL;
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
    P(&sema);
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
    CLIENT *curr = cr->first;
    while(curr != NULL) {
        shutdown(curr->fd,SHUT_RD);
        curr = curr->next;
    }
}