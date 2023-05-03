#include "client_registry.h"
#include "jeux_globals.h"
#include "debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include "player.h"

typedef struct player{
    char *username;
    int ref;
    double rating;
    pthread_mutex_t lockp;
    pthread_mutex_t lock2;
}PLAYER;


PLAYER *player_create(char *name) {
    pthread_mutex_t lockp;
    pthread_mutex_t lock2;
    pthread_mutex_init(&lockp,NULL);
    pthread_mutex_init(&lock2,NULL);
    if(name == NULL) return NULL;
    char* newname = calloc(1,strlen(name)+1);
    memcpy(newname,name,strlen(name));
    newname[strlen(name)] = '\0';
    PLAYER *new = calloc(1,sizeof(PLAYER));
    new->username = newname;
    new->ref=0;
    new->rating = PLAYER_INITIAL_RATING;
    new->lockp = lockp;
    new->lock2 = lock2;
    player_ref(new,"new player created");
    return new;
}

/*
 * Increase the reference count on a player by one.
 *
 * @param player  The PLAYER whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same PLAYER object that was passed as a parameter.
 */
PLAYER *player_ref(PLAYER *player, char *why) {
    if(player == NULL) return NULL;
    pthread_mutex_lock(&player->lock2);
    debug("PLAYER ref: %p, because: %s",player,why);
    player->ref++;
    pthread_mutex_unlock(&player->lock2);
    return player;
}

/*
 * Decrease the reference count on a PLAYER by one.
 * If after decrementing, the reference count has reached zero, then the
 * PLAYER and its contents are freed.
 *
 * @param player  The PLAYER whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void player_unref(PLAYER *player, char *why) {
    if(player != NULL) {
        pthread_mutex_lock(&player->lock2);
        debug("player %p ref: %s",player,why);
        player->ref--;
        if(player->ref == 0) {
            if(player->username != NULL) free(player->username);
            pthread_mutex_unlock(&player->lock2);
            pthread_mutex_destroy(&player->lockp);
            pthread_mutex_destroy(&player->lock2);
            free(player);
        }
        else{
            pthread_mutex_unlock(&player->lock2);
        }
    }
}

/*
 * Get the username of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the username of the player.
 */
char *player_get_name(PLAYER *player) {
    if(player == NULL) return NULL;
    return player->username;
    
}

/*
 * Get the rating of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the rating of the player.
 */
int player_get_rating(PLAYER *player) {
    if(player == NULL) return -1;
    int rating = (int)(round(player->rating));
    return rating;
}

/*
 * Post the result of a game between two players.
 * To update ratings, we use a system of a type devised by Arpad Elo,
 * similar to that used by the US Chess Federation.
 * The player's ratings are updated as follows:
 * Assign each player a score of 0, 0.5, or 1, according to whether that
 * player lost, drew, or won the game.
 * Let S1 and S2 be the scores achieved by player1 and player2, respectively.
 * Let R1 and R2 be the current ratings of player1 and player2, respectively.
 * Let E1 = 1/(1 + 10**((R2-R1)/400)), and
 *     E2 = 1/(1 + 10**((R1-R2)/400))
 * Update the players ratings to R1' and R2' using the formula:
 *     R1' = R1 + 32*(S1-E1)
 *     R2' = R2 + 32*(S2-E2)
 *
 * @param player1  One of the PLAYERs that is to be updated.
 * @param player2  The other PLAYER that is to be updated.
 * @param result   0 if draw, 1 if player1 won, 2 if player2 won.
 */
void player_post_result(PLAYER *player1, PLAYER *player2, int result) {
    if(player1 != NULL && player2 != NULL) {
        pthread_mutex_lock(&player1->lockp);
        pthread_mutex_lock(&player2->lockp);
        double r1 = player_get_rating(player1);
        double r2 = player_get_rating(player2);
        double expo1 = (r2-r1)/400;
        double expo2 = (r1-r2)/400;
        double s1 = result == 0 ? 0.5 : result == 1 ? 1 : 0;
        double s2 = 1-s1;
        double e1 = 1/(1 + pow(10,expo1));
        double e2 = 1/(1 + pow(10,expo2));
        double r1new = (r1 + 32*(s1-e1));
        double r2new = (r2 + 32*(s2-e2));
        debug("player1: %lf player2: %lf",r1new,r2new);
        player1->rating = r1new;
        player2->rating = r2new;
        pthread_mutex_unlock(&player1->lockp);
        pthread_mutex_unlock(&player2->lockp);
    }
}
