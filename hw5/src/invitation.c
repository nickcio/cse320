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
}INVITATION;

INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
    if(source == target) return NULL;
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
    new = inv_ref(new,"creation");
    return new;
}

INVITATION *inv_ref(INVITATION *inv, char *why) {
    if(inv == NULL) return NULL;
    debug("invitation %p ref: %s",inv,why);
    inv->ref++;
    return inv;
}

void inv_unref(INVITATION *inv, char *why) {
    if(inv != NULL) {
        debug("invitation %p ref: %s",inv,why);
        inv->ref--;
        if(inv->ref == 0) free(inv);
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
    GAME *newgame = game_create();
    if(newgame != NULL) inv->game = newgame;
    else return -1;
    return 0;
}

int inv_close(INVITATION *inv, GAME_ROLE role) {
    if(inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE) return -1;
    if(inv->game != NULL) {
        if(game_is_over(inv->game)) {
            inv->state = INV_CLOSED_STATE;
            return 0;
        }
        else{
            if(role == NULL_ROLE) {
                return -1;
            }
            int res = game_resign(inv->game,role);
            if(res) return -1;
            inv->state = INV_CLOSED_STATE;
            return 0;
        }
    }
    else{
        inv->state = INV_CLOSED_STATE;
        return 0;
    }
    return -1;
}