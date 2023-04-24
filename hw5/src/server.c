#include "client_registry.h"
#include "jeux_globals.h"
#include "debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "server.h"

extern CLIENT_REGISTRY *client_registry;

void payloadstart(void **payload) {
    payload[0] = NULL;
    payload[1] = NULL;
}

void payloadre(void **payload) {
    if(payload[0] != NULL) free(payload[0]);
    if(payload[1] != NULL) free(payload[1]);
    payload[0] = NULL;
    payload[1] = NULL;
}

void sendpack(int fd,JEUX_PACKET_HEADER *hdr,void **payload,JEUX_PACKET_TYPE type,size_t size,int role,int id) {
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC,&tspec);
    hdr->type = type;
    hdr->id = id;
    hdr->role = role;
    hdr->timestamp_sec = tspec.tv_sec;
    hdr->timestamp_nsec = tspec.tv_nsec;
    void *pl;
    if(payload == NULL) {
        hdr->size = 0;
        pl = NULL;
    }
    else {
        hdr->size = size;
        pl = *payload;
        debug("PACKET PAYLOAD: %s",(char *)pl);
    }
    if(proto_send_packet(fd,hdr,pl) != 0) {

    }
    else{
        debug("ACKED!");
    }
}

void clientsendpack(CLIENT *player,JEUX_PACKET_HEADER *hdr,void **payload,JEUX_PACKET_TYPE type,size_t size,int role,int id) {
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC,&tspec);
    hdr->type = type;
    hdr->id = id;
    hdr->role = role;
    hdr->timestamp_sec = tspec.tv_sec;
    hdr->timestamp_nsec = tspec.tv_nsec;
    void *pl;
    if(payload == NULL) {
        hdr->size = 0;
        pl = NULL;
    }
    else {
        hdr->size = size;
        pl = *payload;
        debug("PACKET PAYLOAD: %s",(char *)pl);
    }
    if(client_send_packet(player,hdr,pl) != 0) {

    }
    else{
        debug("ACKED!");
    }
}

void *jeux_client_service(void *arg) {
    int fd = *((int*)arg);
    free(arg);
    debug("NUM: %d",fd);
    CLIENT *cli = creg_register(client_registry,fd);
    void **payload = calloc(1,sizeof(void*)*2);
    payloadstart(payload);
    void **ps = payload;
    JEUX_PACKET_HEADER *hdr = calloc(1,sizeof(JEUX_PACKET_HEADER));
    int loggedin = 0;
    int terminate = 0;
    while(!terminate) {
        payload = ps;
        debug("START");
        if(proto_recv_packet(fd,hdr,payload) != 0) {
            terminate = 1;
            debug("NOT");
        }
        else{
            debug("RECVD");
            JEUX_PACKET_TYPE t = hdr->type;
            if(!loggedin) {
                if(t==JEUX_LOGIN_PKT) {
                    char *playername = payload[0];
                    debug("11!");
                    PLAYER *newplayer = player_create(playername);
                    debug("12! %p %p %s",cli,newplayer,playername);
                    if(client_login(cli,newplayer) != 0) {
                        debug("IN!");
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        debug("IN GOOD!");
                        loggedin = 1;
                        sendpack(fd,hdr,NULL,JEUX_ACK_PKT,0,0,0);
                    }
                    debug("13!");
                }
                else{
                    debug("Log in first");
                    sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                }
            }
            else{
                if(t==JEUX_LOGIN_PKT) {
                    debug("Already logged in");
                    sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                }
                else if(t==JEUX_USERS_PKT) {
                    debug("OK");
                    PLAYER** players = creg_all_players(client_registry);
                    FILE *fp;
                    size_t size;
                    char* buff;
                    debug("OK2");
                    fp = open_memstream(&buff,&size);
                    PLAYER** ps = players;
                    while(*ps != NULL) {
                        debug("Stringx: %s\t%d\n",player_get_name(*ps),player_get_rating(*ps));
                        fprintf(fp,"%s\t%d\n",player_get_name(*ps),player_get_rating(*ps));
                        ps++;
                    }
                    fflush(fp);
                    debug("String: %s, size: %ld",buff,size);
                    sendpack(fd,hdr,(void**)(&buff),JEUX_ACK_PKT,size,0,0);
                    fclose(fp);
                    free(buff);
                    free(players);
                }
                else if(t==JEUX_INVITE_PKT) {
                    debug("OK");
                    char *targetname = payload[0];
                    debug("TARGET: %s",targetname);
                    int role = hdr->role;
                    debug("ROLE: %d",role);
                    CLIENT *oc = creg_lookup(client_registry,targetname);
                    debug("OK3");
                    if(oc == NULL) {
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        int self = role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
                        debug("OK4 %p %p %d %d",cli,oc,self,role);
                        int inv = client_make_invitation(cli,oc,self,role);
                        debug("OK5");
                        sendpack(fd,hdr,NULL,JEUX_ACK_PKT,0,0,inv);
                    }
                }
                else if(t==JEUX_REVOKE_PKT) {
                    debug("OK");
                    int id = hdr->id;
                    int fail = client_revoke_invitation(cli,id);
                    if(fail) {
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        sendpack(fd,hdr,NULL,JEUX_ACK_PKT,0,0,0);
                    }
                }
                else if(t==JEUX_DECLINE_PKT) {
                    debug("OK");
                    int id = hdr->id;
                    int fail = client_decline_invitation(cli,id);
                    if(fail) {
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        sendpack(fd,hdr,NULL,JEUX_ACK_PKT,0,0,0);
                    }
                }
                else if(t==JEUX_ACCEPT_PKT) {
                    debug("OK");
                    int id = hdr->id;
                    char* start = NULL;
                    int fail = client_accept_invitation(cli,id,&start);
                    if(fail) {
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        sendpack(fd,hdr,(void**)&start,JEUX_ACK_PKT,strlen(start),0,0);
                        free(start);
                    }
                }
                else if(t==JEUX_MOVE_PKT) {
                    debug("OK");
                    char *move = payload[0];
                    int id = hdr->id;
                    int fail = client_make_move(cli,id,move);
                    if(fail) {
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        sendpack(fd,hdr,NULL,JEUX_ACK_PKT,0,0,0);
                    }
                }
                else if(t==JEUX_RESIGN_PKT) {
                    debug("OK");
                    int id = hdr->id;
                    int fail = client_resign_game(cli,id);
                    if(fail) {
                        sendpack(fd,hdr,NULL,JEUX_NACK_PKT,0,0,0);
                    }
                    else{
                        sendpack(fd,hdr,NULL,JEUX_ACK_PKT,0,0,0);
                    }
                }
            }
            
        }
        debug("STILL HERE");
        payloadre(ps);
    }
    debug("DONE");
    return NULL;
}