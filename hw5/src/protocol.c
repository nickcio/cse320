#include "protocol.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
/*
 * Send a packet, which consists of a fixed-size header followed by an
 * optional associated data payload.
 *
 * @param fd  The file descriptor on which packet is to be sent.
 * @param hdr  The fixed-size packet header, with multi-byte fields
 *   in network byte order
 * @param data  The data payload, or NULL, if there is none.
 * @return  0 in case of successful transmission, -1 otherwise.
 *   In the latter case, errno is set to indicate the error.
 *
 * All multi-byte fields in the packet are assumed to be in network byte order.
 */
int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
    //get size
    uint16_t data_size;
    if(data == NULL) data_size = 0;
    else data_size = hdr->size;

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

/*
 * Receive a packet, blocking until one is available.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param hdr  Pointer to caller-supplied storage for the fixed-size
 *   packet header.
 * @param datap  Pointer to a variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * The returned packet has all multi-byte fields in network byte order.
 * If the returned payload pointer is non-NULL, then the caller has the
 * responsibility of freeing that storage.
 */
int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
    void *temp = hdr;
    size_t bs = sizeof(JEUX_PACKET_HEADER);
    while(bs > 0) {
        ssize_t total = read(fd,temp,bs);
        if(total == -1 || total == 0) return -1;
        bs-=total;
        temp+=total;
    }

    uint32_t timesec = ntohl(hdr->timestamp_sec);
    uint32_t timensec = ntohl(hdr->timestamp_nsec);
    hdr->timestamp_sec = timesec;
    hdr->timestamp_nsec = timensec;

    uint16_t data_size = hdr->size;
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