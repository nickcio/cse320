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

void addfl(sf_block *block, int i) { //Adds block to free list i
    sf_block *nexter = sf_free_list_heads[i].body.links.next;
    block->body.links.next = nexter;
    block->body.links.prev = &sf_free_list_heads[i];
    nexter->body.links.prev = block;
    sf_free_list_heads[i].body.links.next = block;
}

void findadd(sf_block *p) { //Find where to add a freed block, and add it
    size_t psize = (p->header/8 << 3);
    int i = 0;
    for(size_t min = MIN_BLOCK_SIZE; min < psize && i < NUM_FREE_LISTS; min*=2) i++; //Find correct list
    addfl(p,i);
}

void delfl(sf_block *block) { //Removes block from free list
    sf_block *prev = block->body.links.prev;
    sf_block *next = block->body.links.next;
    prev->body.links.next = next;
    next->body.links.prev = prev;
}

void *getfooter(sf_block *p) {//Assumes block is free
    size_t psize = (p->header/8 << 3);
    void *footer = (void *)(p) + psize - 8;
    return footer;
}

void newmem() { //Memgrow + coalescing
    int init = 0;
    if(sf_mem_start() == sf_mem_end()) init = 1; //Whether this is the first page allocated, to know if we have to make prologue
    void *s = sf_mem_grow(); //Allocated page of memory
    if(init) { //Make prologue
        sf_block prolog; //Prologue
        prolog.header = (sf_header)(32 | THIS_BLOCK_ALLOCATED); //Header for 32 byte block, alloc flag == 1
        *((sf_block*)s) = prolog;
        sf_show_block(s);

        sf_block *bigfree = s+32; //Pointer to first free block
        bigfree->header = (sf_header)((PAGE_SZ-MIN_BLOCK_SIZE-8)); //Sets big sized free block, alloc flag = 0
        sf_footer *bigfoot = s+32+(PAGE_SZ-MIN_BLOCK_SIZE-16);
        *bigfoot = (sf_footer)(bigfree->header);

        for(int i = 0; i < NUM_FREE_LISTS; i++) { //Init free lists
            sf_block dummy;
            dummy.body.links.next = &sf_free_list_heads[i];
            dummy.body.links.prev = &sf_free_list_heads[i];
            sf_free_list_heads[i] = dummy;
        }

        addfl(bigfree,9); //Add block to free list 9
    }
    sf_block *end = (sf_mem_end()-sizeof(sf_header)); //Where epilogue will be
    //fprintf(stderr,"end pointer: %p, epilog p: %p, diff: %ld\n",sf_mem_end(),end,((long int)sf_mem_end()-(long int)end));
    sf_block epilog;
    sf_header epih = 0 | THIS_BLOCK_ALLOCATED;
    epilog.header = epih;
    *end=epilog; //Sets epilogue
    fprintf(stderr,"size of header: %ld, %ld\n",sizeof(sf_header), ((long int)s)%8);
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

void *malsplit(sf_block *p, size_t size) { //Allocates part of a free block, assumes size < size of block pointed to by p
    size_t psize = (p->header/8 << 3); //size of block pointed to by p
    delfl(p);
    if(psize - size < 32) { //Splitting the block will result in splintering, so allocate whole block!
        sf_block newb;
        newb.header = (sf_header)((psize) | THIS_BLOCK_ALLOCATED);
        *p = newb;
        return p;
    }
    size_t qsize = (psize - size) >> 3 << 3;
    sf_block *newfree = (sf_block *)((void *)p+size);
    fprintf(stderr,"QSIZE: %ld, SIZE: %ld, P: %p, NEWFREE: %p\n",qsize,size,p,newfree);
    sf_block freeb;
    freeb.header = (sf_header)(qsize);
    *newfree = freeb;
    sf_footer *freefooter = (sf_footer *)getfooter(newfree);
    *freefooter = (sf_footer)(qsize);
    findadd(newfree);
    
    sf_block newb;
    newb.header = (sf_header)((size) | THIS_BLOCK_ALLOCATED);
    *p = newb;
    return p;
}

void *findfree(size_t size, int i) { //Attempts to find a free block of at least size in free list i
    sf_block *head = &sf_free_list_heads[i];
    sf_block *this = head->body.links.next;
    while(this != head) {
        if((this->header >> 3) >= size) {
            return this;
        }
        this = this->body.links.next;
    }
    return NULL;
}

void *checkfree(size_t size) { //Finds a free block of at least size by checking free lists
    int i = 0;
    for(size_t min = MIN_BLOCK_SIZE; min < size && i < NUM_FREE_LISTS; min*=2) { //i will be first list to search
        i++;
    }
    for(i = i; i < NUM_FREE_LISTS; i++) {
        void *pp = findfree(size,i);
        if(pp != NULL) {
            return pp;
        }
    }
    return NULL;
}

void *sf_malloc(size_t size) {
    if(size == 0) return NULL; //Size 0, return NULL
    size_t fullsize = size%8 == 0 ? size + 8 : (size + 16)+(8-size%8); //Round to multiple of 8, fullsize is size with header
    if(sf_mem_start() == sf_mem_end()) { //If total mem pages is 0, mem grow
        newmem();
        totalsz+=PAGE_SZ;
    }
    void *start = sf_mem_start(); //Start of mem pages

    //Check if it is valid for quick lists
    //void *g = (void *)0;
    if(fullsize >= MIN_BLOCK_SIZE && fullsize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8) {
        int qind = (fullsize - MIN_BLOCK_SIZE)/8;
        fprintf(stderr,"qind: %d\n",qind);
        //g = addql(qind);
        fprintf(stderr,"after\n");
    }
    fprintf(stderr,"memstart: %p\n",start);
    fprintf(stderr,"size: %ld newsize: %ld\n",size,fullsize);
    fprintf(stderr,"durr: %p, %ld\n",start,totalsz);
    //fprintf(stderr,"found pointer: %p\n",checkfree(fullsize));
    void *g = checkfree(fullsize);
    if(g != NULL) {
        malsplit(g,fullsize);
    }

    sf_show_heap();
    return g;
}

void sf_free(void *pp) {
    fprintf(stderr,"SF_FREE %p\n",pp);
}

void *sf_realloc(void *pp, size_t rsize) {
    // TO BE IMPLEMENTED
    abort();
}

void *sf_memalign(size_t size, size_t align) {
    // TO BE IMPLEMENTED
    abort();
}
