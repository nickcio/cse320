#include "protocol.h"
#include "debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
    //get size
    debug("SEND PACKET");
    uint16_t data_size;
    if(data == NULL) data_size = 0;
    else {
        data_size = htons(hdr->size);
        hdr->size = data_size;
    }


    uint32_t timesec = htonl(hdr->timestamp_sec);
    uint32_t timensec = htonl(hdr->timestamp_nsec);
    hdr->timestamp_sec = timesec;
    hdr->timestamp_nsec = timensec;

    void *temp = hdr;
    size_t bs = sizeof(JEUX_PACKET_HEADER);
    while(bs > 0) {
        ssize_t total = write(fd,temp,bs);
        if(total == -1) return -1;
        bs-=total;
        temp+=total;
    }
    if(!data_size) return 0;

    void *td = data;
    size_t ds = data_size;
    while(ds > 0) {
        ssize_t total = write(fd,td,ds);
        if(total == -1) return -1;
        ds-=total;
        td+=total;
    }

    return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
    //fprintf(stderr,"Hello error\n");
    debug("RECV PACKET");
    void *temp = hdr;
    size_t bs = sizeof(JEUX_PACKET_HEADER);
    while(bs > 0) {
        ssize_t total = read(fd,temp,bs);
        debug("READ SIZE: %ld",total);
        if(total == -1 || total == 0) return -1;
        bs-=total;
        temp+=total;
    }

    uint32_t timesec = ntohl(hdr->timestamp_sec);
    uint32_t timensec = ntohl(hdr->timestamp_nsec);
    hdr->timestamp_sec = timesec;
    hdr->timestamp_nsec = timensec;

    uint16_t data_size = ntohs(hdr->size);
    hdr->size = data_size;
    debug("Received packet - Type: %d, Size: %d\n", hdr->type, hdr->size);
    if(!data_size) {
        payloadp = NULL;
        return 0;
    }

    void *pp = calloc(1,data_size);
    void *td = pp;
    size_t ds = data_size;
    while(ds > 0) {
        ssize_t total = read(fd,td,ds);
        if(total == -1 || total == 0) return -1;
        ds-=total;
        td+=total;
    }
    *payloadp = pp;

    return 0;
}