#include "player_registry.h"
#include "jeux_globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "csapp.h"
#include "debug.h"

typedef struct player_registry {
    int playercount;
    PLAYER *players[MAX_CLIENTS*3];
    pthread_mutex_t lock;
}PLAYER_REGISTRY;

/*
 * Initialize a new player registry.
 *
 * @return the newly initialized PLAYER_REGISTRY, or NULL if initialization
 * fails.
 */

PLAYER_REGISTRY *preg_init(void) {
    pthread_mutex_t lock;
    pthread_mutex_init(&lock,NULL);
    PLAYER_REGISTRY *preg = calloc(1,sizeof(PLAYER_REGISTRY));
    preg->players[0] = NULL;
    preg->playercount=0;
    preg->lock = lock;
    return preg;
}

/*
 * Finalize a player registry, freeing all associated resources.
 *
 * @param cr  The PLAYER_REGISTRY to be finalized, which must not
 * be referenced again.
 */
void preg_fini(PLAYER_REGISTRY *preg) {
    pthread_mutex_lock(&preg->lock);
    if(preg != NULL) {
        if(preg->players != NULL) {
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(preg->players[i] != NULL) player_unref(preg->players[i],"preg fini");
            }
        }
        free(preg);
    }
    pthread_mutex_unlock(&preg->lock);
    pthread_mutex_destroy(&preg->lock);
}

/*
 * Register a player with a specified user name.  If there is already
 * a player registered under that user name, then the existing registered
 * player is returned, otherwise a new player is created.
 * If an existing player is returned, then its reference count is increased
 * by one to account for the returned pointer.  If a new player is
 * created, then the returned player has reference count equal to two:
 * one count for the pointer retained by the registry and one count for
 * the pointer returned to the caller.
 *
 * @param name  The player's user name, which is copied by this function.
 * @return A pointer to a PLAYER object, in case of success, otherwise NULL.
 *
 */
PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
    if(preg == NULL || name == NULL) return NULL;
    pthread_mutex_lock(&preg->lock);
    int firstnull = -1;
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(preg->players[i] != NULL) {
            if(strcmp(name,player_get_name(preg->players[i])) == 0) {
                pthread_mutex_unlock(&preg->lock);
                return preg->players[i];
            }
        }
        else if(firstnull == -1) firstnull = i;
    }
    if(firstnull == -1) {
        pthread_mutex_unlock(&preg->lock);
        return NULL;
    }
    PLAYER *new = player_create(name);
    preg->players[firstnull] = new;
    new = player_ref(new,"reg");
    preg->playercount++;
    pthread_mutex_unlock(&preg->lock);
    return new;
}