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

void sendpack(int fd,JEUX_PACKET_HEADER *hdr,void **payload,JEUX_PACKET_TYPE type) {
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC,&tspec);
    hdr->type = type;
    hdr->size = 0;
    hdr->timestamp_sec = tspec.tv_sec;
    hdr->timestamp_nsec = tspec.tv_nsec;
    void* empty = NULL;
    debug("GOT TO ACK!");
    if(proto_send_packet(fd,hdr,empty) != 0) {

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
            //terminate = 1;
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
                        sendpack(fd,hdr,payload,JEUX_NACK_PKT);
                    }
                    else{
                        debug("IN GOOD!");
                        sendpack(fd,hdr,payload,JEUX_ACK_PKT);
                    }
                    debug("13!");
                }
                else{
                    debug("Log in first");
                    sendpack(fd,hdr,payload,JEUX_NACK_PKT);
                }
            }
            else{
                if(t==JEUX_LOGIN_PKT) {
                    debug("Already logged in");
                    sendpack(fd,hdr,payload,JEUX_NACK_PKT);
                }
                else{
                    debug("OK");
                    sendpack(fd,hdr,payload,JEUX_ACK_PKT);
                }
            }
            
        }
        debug("STILL HERE");
        payloadre(ps);
    }
    debug("DONE");
    return NULL;
}