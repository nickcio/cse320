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
#include <regex.h>
#include <ctype.h>
#include "game.h"

typedef struct game {
    int ref; //Refs
    int turn; //Turn number, used to know when who should go, starts at 0
    int isover; //1 if game is over 0 if otherwise
    GAME_ROLE winner; //whoever won game
    char state[9]; //Represents 9 squares of a tic tac toe board
    pthread_mutex_t lockg; //Lock for most functions
    pthread_mutex_t lockr; //Lock for refs
}GAME;

typedef struct game_move {
    GAME_ROLE role; //role of player making move
    int spot; //where player is making their move
}GAME_MOVE;

GAME *game_create(void) {
    pthread_mutex_t lockg; //Lock for most functions
    pthread_mutex_t lockr; //Lock for refs
    pthread_mutex_init(&lockg,NULL);
    pthread_mutex_init(&lockr,NULL);
    GAME *new = calloc(1,sizeof(GAME));
    new->ref = 0;
    new->turn = 0;
    new->isover = 0;
    new->winner = NULL_ROLE;
    new->lockg = lockg;
    new->lockr = lockr;
    game_ref(new,"game created");
    return new;
}

GAME *game_ref(GAME *game, char *why) {
    if(game == NULL) return NULL;
    pthread_mutex_lock(&game->lockr);
    debug("Game %p ref: %s",game,why);
    game->ref++;
    pthread_mutex_unlock(&game->lockr);
    return game;
}

void game_unref(GAME *game, char *why) {
    if(game != NULL) {
        pthread_mutex_lock(&game->lockr);
        debug("game %p unref: %s",game,why);
        game->ref--;
        if(game->ref == 0) {
            pthread_mutex_unlock(&game->lockr);
            pthread_mutex_destroy(&game->lockg);
            pthread_mutex_destroy(&game->lockr);
            free(game);
        }
        else pthread_mutex_unlock(&game->lockr);
    }
}

//return 1 for victory for role, 0 for not
int check_victory(GAME *game, GAME_ROLE role) {
    if(game == NULL || role == NULL_ROLE) return 0;
    char* state = game->state;
    //Horizontal
    if(state[0] == role && state[1] == role && state[2] == role) return 1;
    if(state[3] == role && state[4] == role && state[5] == role) return 1;
    if(state[6] == role && state[7] == role && state[8] == role) return 1;
    //Vertical
    if(state[0] == role && state[3] == role && state[6] == role) return 1;
    if(state[1] == role && state[4] == role && state[7] == role) return 1;
    if(state[2] == role && state[5] == role && state[8] == role) return 1;
    //Diag
    if(state[0] == role && state[4] == role && state[8] == role) return 1;
    if(state[2] == role && state[4] == role && state[6] == role) return 1;
    return 0;
}

int game_apply_move(GAME *game, GAME_MOVE *move) {
    if(game == NULL || move == NULL || game->isover) return -1;
    if(move->role == NULL_ROLE) return -1;
    pthread_mutex_lock(&game->lockg);
    GAME_ROLE current = 1+(game->turn%2); //Whoevers turn it should be
    if(move->role != current) {
        pthread_mutex_unlock(&game->lockg);
        return -1;
    }
    int spot = move->spot;
    if(spot < 0 || spot > 8) {
        pthread_mutex_unlock(&game->lockg);
        return -1;
    }
    if(game->state[spot] != 0) {
        pthread_mutex_unlock(&game->lockg);
        return -1;
    }
    game->state[spot] = current;
    if(check_victory(game,current)) {
        game->isover = 1;
        game->winner = current;
        pthread_mutex_unlock(&game->lockg);
        return 0;
    }
    game->turn++;
    if(game->turn >= 9 && game->isover != 1) {
        game->isover = 1;
        game->winner = NULL_ROLE;
        pthread_mutex_unlock(&game->lockg);
        return 0;
    }
    pthread_mutex_unlock(&game->lockg);
    return 0;
}

int game_resign(GAME *game, GAME_ROLE role) {
    if(game == NULL || game->isover) return -1;
    pthread_mutex_lock(&game->lockg);
    game->isover = 1;
    if(role == FIRST_PLAYER_ROLE) game->winner = SECOND_PLAYER_ROLE;
    else if(role == SECOND_PLAYER_ROLE) game->winner = FIRST_PLAYER_ROLE;
    else game->winner = NULL_ROLE;
    pthread_mutex_unlock(&game->lockg);
    return 0;
}

//directly turns 0 to ' ', 1 to 'X', 2 to 'O'
char parse_spot(char spot) {
    switch(spot) {
        case 0: return ' ';
        case 1: return 'X';
        case 2: return 'O';
        default: return -1;
    }
    return -1;
}

//reverse of parse spot
char unparse_spot(char spot) {
    switch(spot) {
        case ' ': return 0;
        case 'X': return 1;
        case 'O': return 2;
        default: return -1;
    }
    return -1;
}

char *game_unparse_state(GAME *game) {
    pthread_mutex_lock(&game->lockg);
    FILE *fp;
    char *buffer;
    size_t bsize;
    if((fp = open_memstream(&buffer,&bsize)) == NULL) {
        pthread_mutex_unlock(&game->lockg);
        return NULL;
    }
    GAME_ROLE current = 1+(game->turn%2); //Whoevers turn it should be
    char *state = game->state;
    fprintf(fp,"%c|%c|%c\n",parse_spot(state[0]),parse_spot(state[1]),parse_spot(state[2]));
    fprintf(fp,"-----\n");
    fprintf(fp,"%c|%c|%c\n",parse_spot(state[3]),parse_spot(state[4]),parse_spot(state[5]));
    fprintf(fp,"-----\n");
    fprintf(fp,"%c|%c|%c\n",parse_spot(state[6]),parse_spot(state[7]),parse_spot(state[8]));
    fprintf(fp,"%c to move\n",parse_spot(current));
    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&game->lockg);
    return buffer;
}

int game_is_over(GAME *game) {
    if(game == NULL) return 0;
    return game->isover;
}

GAME_ROLE game_get_winner(GAME *game) {
    if(game == NULL) return NULL_ROLE;
    return game->winner;
}

GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str) {
    if(game == NULL) return NULL;
    pthread_mutex_lock(&game->lockg);
    char *newstr = calloc(1,strlen(str)+1);
    memcpy(newstr,str,strlen(str));
    str = newstr;
    regex_t regs;
    int regerr = regcomp(&regs,"^(\\s)*[1-9](\\s)*$",REG_EXTENDED);
    if(regerr) {
        pthread_mutex_unlock(&game->lockg);
        free(str);
        return NULL;
    }
    regex_t regl;
    regerr = regcomp(&regl,"^(\\s)*[1-9](<-[XO])(\\s)*$",REG_EXTENDED);
    if(regerr) {
        regfree(&regs);
        pthread_mutex_unlock(&game->lockg);
        free(str);
        return NULL;
    }

    int val = -2;
    if((val = regexec(&regl,str,0,NULL,0)) == 0) {
        char *tp = str;
        while(isspace(*tp)) tp++;
        int spot = atoi(tp)-1;
        tp+=3;
        GAME_ROLE thisrole = role;
        if(thisrole != NULL_ROLE) {
            if(thisrole != unparse_spot(*tp)) {
                regfree(&regl);
                regfree(&regs);
                pthread_mutex_unlock(&game->lockg);
                free(str);
                return NULL;
            }
        }
        else{
            thisrole = unparse_spot(*tp);
        }
        GAME_MOVE *new = calloc(1,sizeof(GAME_MOVE));
        new->role = thisrole;
        new->spot = spot;
        regfree(&regl);
        regfree(&regs);
        pthread_mutex_unlock(&game->lockg);
        free(str);
        return new;
    }
    if((val = regexec(&regs,str,0,NULL,0)) == 0) {
        char *tp = str;
        while(isspace(*tp)) tp++;
        int spot = atoi(tp)-1;
        GAME_ROLE thisrole = role;
        if(thisrole == NULL_ROLE) {
            regfree(&regl);
            regfree(&regs);
            pthread_mutex_unlock(&game->lockg);
            free(str);
            return NULL;
        }
        GAME_MOVE *new = calloc(1,sizeof(GAME_MOVE));
        new->role = thisrole;
        new->spot = spot;
        regfree(&regl);
        regfree(&regs);
        pthread_mutex_unlock(&game->lockg);
        free(str);
        return new;
    }
    regfree(&regl);
    regfree(&regs);
    pthread_mutex_unlock(&game->lockg);
    free(str);
    return NULL;
}

char *game_unparse_move(GAME_MOVE *move) {
    if(move == NULL) return NULL;
    char *new = calloc(5,sizeof(char));
    new[0] = (move->spot)+49;
    new[1] = '<';
    new[2] = '-';
    new[3] = parse_spot(move->role);
    return new;
}