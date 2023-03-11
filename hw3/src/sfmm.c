/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"

static size_t totalsz = 0;
static size_t allocsz = 0;
static size_t MIN_BLOCK_SIZE = 32;

void newmem() { //Memgrow + coalescing
    int init = 0;
    if(sf_mem_start() == sf_mem_end()) init = 1; //Whether this is the first page allocated, to know if we have to make prologue
    void *s = sf_mem_grow(); //Allocated page of memory
    if(init) { //Make prologue
        sf_block prolog;
        sf_header proh = (32 << 3)+1; //Header for 32 byte block, alloc flag == 1
        prolog.header = proh;
    }
}

void *addql(int index) { //Assumes index is valid
    fprintf(stderr,"atblock\n");
    int len = sf_quick_lists[index].length;
    fprintf(stderr,"len: %d\n",len);
    if(len == 0) {
        sf_header nh = (index*8+MIN_BLOCK_SIZE)/8 << 3; //Header block_size is first 61 bits
        nh = nh | 0x5; //Alloc bit 4 & 1 (qklst and alloc)
        sf_block newb; //New block
        newb.header = nh; //Set header
        sf_quick_lists[index].first = &newb;
        len++;
        allocsz+= (index*8+MIN_BLOCK_SIZE);
    }
    sf_block *ql = sf_quick_lists[index].first;
    fprintf(stderr,"this block: %d %ld\n",len,ql->header);
    sf_show_block(ql);
    //sf_show_quick_lists();
    return NULL;
}

void *sf_malloc(size_t size) {
    if(size == 0) return NULL; //Size 0, return NULL
    size_t fullsize = size%8 == 0 ? size + 16 : (size + 16)+(8-size%8); //Round to multiple of 8, fullsize is size with header
    if(sf_mem_start() == sf_mem_end()) { //If total mem pages is 0, mem grow
        newmem();
        totalsz+=PAGE_SZ;
    }
    void *start = sf_mem_start(); //Start of mem pages
    //sf_header nh = fullsize/8 << 3; //Header block_size is first 61 bits, so shift fullsize
    //nh = nh | 0x1; //Alloc bit 1
    //sf_block newb; //New block
    //newb.header = nh; //Set header
    //Check if it is valid for quick lists
    void *g = 0;
    if(fullsize >= MIN_BLOCK_SIZE && fullsize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8) {
        int qind = (fullsize - MIN_BLOCK_SIZE)/8;
        fprintf(stderr,"qind: %d\n",qind);
        g = addql(qind);
        fprintf(stderr,"after\n");
    }
    fprintf(stderr,"memstart: %p\n",start);
    fprintf(stderr,"size: %ld newsize: %ld\n",size,fullsize);
    fprintf(stderr,"durr: %p, %ld\n",start,totalsz);
    //sf_show_block(&newb);
    return g;
}

void sf_free(void *pp) {
    // TO BE IMPLEMENTED
    abort();
}

void *sf_realloc(void *pp, size_t rsize) {
    // TO BE IMPLEMENTED
    abort();
}

void *sf_memalign(size_t size, size_t align) {
    // TO BE IMPLEMENTED
    abort();
}
