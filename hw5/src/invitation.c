#include "jeux_globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "debug.h"
#include "invitation.h"

typedef struct invitation{
    INVITATION_STATE state;
    int ref;
    CLIENT *source;
    CLIENT *target;
    GAME_ROLE srole;
    GAME_ROLE trole;
    GAME *game;
    pthread_mutex_t locki;
    pthread_mutex_t lock2;
}INVITATION;


INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
    if(source == target) return NULL;
    pthread_mutex_t locki;
    pthread_mutex_t lock2;
    pthread_mutex_init(&locki,NULL);
    pthread_mutex_init(&lock2,NULL);
    INVITATION *new = calloc(1,sizeof(INVITATION));
    new->state = INV_OPEN_STATE;
    new->ref = 0;
    new->source = source;
    new->target = target;
    client_ref(source,"inv source");
    client_ref(target,"inv target");
    new->srole = source_role;
    new->trole = target_role;
    new->game = NULL;
    new->locki = locki;
    new->lock2 = lock2;
    //new = inv_ref(new,"creation");
    return new;
}

INVITATION *inv_ref(INVITATION *inv, char *why) {
    if(inv == NULL) return NULL;
    pthread_mutex_lock(&inv->lock2);
    debug("invitation %p ref: %s",inv,why);
    inv->ref++;
    pthread_mutex_unlock(&inv->lock2);
    return inv;
}

void inv_unref(INVITATION *inv, char *why) {
    if(inv != NULL) {
        pthread_mutex_lock(&inv->lock2);
        debug("invitation %p ref: %d to %d, %s",inv,inv->ref, inv->ref-1,why);
        inv->ref--;
        if(inv->ref == 0) {
            game_unref(inv->game,"game inv free");
            client_unref(inv->source,"source inv free");
            client_unref(inv->target,"target inv free");
            free(inv);
            pthread_mutex_unlock(&inv->lock2);
            pthread_mutex_destroy(&inv->locki);
            pthread_mutex_destroy(&inv->lock2);
        }
        else pthread_mutex_unlock(&inv->lock2);
    }
}

CLIENT *inv_get_source(INVITATION *inv) {
    if(inv == NULL) return NULL;
    return inv->source;
}

CLIENT *inv_get_target(INVITATION *inv) {
    if(inv == NULL) return NULL;
    return inv->target;
}

GAME_ROLE inv_get_source_role(INVITATION *inv) {
    if(inv == NULL) return NULL_ROLE;
    return inv->srole;
}

GAME_ROLE inv_get_target_role(INVITATION *inv) {
    if(inv == NULL) return NULL_ROLE;
    return inv->trole;
}

GAME *inv_get_game(INVITATION *inv){
    if(inv == NULL) return NULL;
    return inv->game;
}

int inv_accept(INVITATION *inv) {
    if(inv->state != INV_OPEN_STATE) return -1;
    pthread_mutex_lock(&inv->locki);
    GAME *newgame = game_create();
    if(newgame != NULL) {
        inv->game = newgame;
    }
    else {
        pthread_mutex_unlock(&inv->locki);
        return -1;
    }
    pthread_mutex_unlock(&inv->locki);
    return 0;
}

int inv_close(INVITATION *inv, GAME_ROLE role) {
    pthread_mutex_lock(&inv->locki);
    if(inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE) {
        pthread_mutex_unlock(&inv->locki);
        return -1;
    }
    if(inv->game != NULL) {
        if(game_is_over(inv->game)) {
            inv->state = INV_CLOSED_STATE;
            pthread_mutex_unlock(&inv->locki);
            return 0;
        }
        else{
            if(role == NULL_ROLE) {
                pthread_mutex_unlock(&inv->locki);
                return -1;
            }
            int res = game_resign(inv->game,role);
            if(res) return -1;
            inv->state = INV_CLOSED_STATE;
            pthread_mutex_unlock(&inv->locki);
            return 0;
        }
    }
    else{
        inv->state = INV_CLOSED_STATE;
        pthread_mutex_unlock(&inv->locki);
        return 0;
    }
    pthread_mutex_unlock(&inv->locki);
    return -1;
}