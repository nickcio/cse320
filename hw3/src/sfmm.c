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
    if((block->header&THIS_BLOCK_ALLOCATED)) return;
    if((block->body.links.next==(sf_block *)0x1000)) return;
    sf_block *prev = block->body.links.prev;
    sf_block *next = block->body.links.next;
    prev->body.links.next = next;
    next->body.links.prev = prev;
    block->body.links.next=NULL;
    block->body.links.prev=NULL;
}

sf_block *getnextblock(sf_block *b) {//Gets next block in heap after this one
    size_t bsize = (b->header/8 << 3);
    void *block = (void *)(b) + bsize;
    return (sf_block *)block;
}

void *getfooter(sf_block *p) {//Assumes block is free
    size_t psize = (p->header/8 << 3);
    void *footer = (void *)(p) + psize - 8;
    return footer;
}

void *getheader(sf_footer *f) {//Gets header FROM POINTER TO FOOTER!
    size_t fsize = ((*f)/8 << 3);
    void *header = (void *)(f) - fsize + 8;
    return header;
}

void refreshpal(sf_block *s) {//s is start of the entire heap. refreshes all prev alloc flags.
    sf_block *curr = getnextblock(s);
    size_t ssize = (s->header/8 << 3);
    int al = 0;
    int pal = 1;
    while(ssize) {
        sf_header head = curr->header;
        ssize = (head/8 << 3);
        //fprintf(stderr,"THISSIZE! %ld",ssize);
        if(head & THIS_BLOCK_ALLOCATED) al = 1;
        else al = 0;
        if(pal) head|=PREV_BLOCK_ALLOCATED;
        else if (head&PREV_BLOCK_ALLOCATED) head-=PREV_BLOCK_ALLOCATED;
        if(!al) {
            sf_footer *footer = (sf_footer*)getfooter(curr);
            *footer = (sf_footer)head;
        }
        curr->header = head;
        curr = getnextblock(curr);
        pal = al;
    }
}

size_t nextfree(sf_block *s) {//Finds if next block is free in heap, if found returns its size
    sf_block *next = getnextblock(s);
    size_t nextsize = (next->header/8 << 3);
    if(!((next->header)&THIS_BLOCK_ALLOCATED)) {
        fprintf(stderr,"NEXT IS FREE!\n");
        sf_show_block(next);
        return nextsize;
    }
    return 0;
}

sf_block *prevfree(sf_block *s) {//Finds if prev block is free in the heap, returns pointer
    fprintf(stderr,"Got here 1! %ld\n",(s->header&PREV_BLOCK_ALLOCATED));
    if((s->header&PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED) return NULL;
    sf_header *head = (sf_header *)getheader((void *)(s)-8);
    fprintf(stderr,"Got here 3!\n");
    if((*head&THIS_BLOCK_ALLOCATED) == 0) return (sf_block *)head;
    fprintf(stderr,"Got here 4!\n");
    return NULL;
}

void delfree(sf_block *s) {//Removes header & footer of free block
    sf_footer *footer = (sf_footer*)getfooter(s);
    *footer = 0;
    s->header = 0;
    s->body.links.next = 0;
    s->body.links.prev = 0;
}

void coalesce(sf_block *s) {//s is start of the entire heap. coalesces entire heap.
    size_t currsize = (s->header/8 << 3);
    size_t nextsize = nextfree(s);
    sf_block *prevpoint = prevfree(s);
    //sf_show_heap();
    fprintf(stderr,"FREEING %p, NEXTSIZE: %ld, PREVPOINT: %p\n",s,nextsize,prevpoint);
    if(!nextsize && !prevpoint) return;
    fprintf(stderr,"Past return! %ld %p\n",currsize, s);
    delfl(s);
    //fprintf(stderr,"Past return2!\n");
    if(nextsize) {
        delfl(getnextblock(s));
    }
    //fprintf(stderr,"Past return3!\n");
    if(prevpoint) {
        delfl(prevpoint);
        delfree(s);
        s = prevpoint;
        nextsize+=currsize;
        fprintf(stderr,"Inprevpoint!\n");
    }
    //fprintf(stderr,"Past return4!\n");
    sf_block newblock;
    sf_header newhead = (sf_header)((s->header)+nextsize);
    newblock.header = newhead;
    *s = newblock;
    fprintf(stderr,"Past return44 %ld %ld %ld!\n",s->header,nextsize,newhead);
    sf_footer *footer = (sf_footer*)getfooter(s);
    fprintf(stderr,"Past return25!\n");
    *footer = (sf_footer)newhead;
    findadd(s);
    fprintf(stderr,"Past return5!\n");
    //sf_show_heap();
}

void newmem() { //Memgrow + coalescing
    int init = 0;
    if(sf_mem_start() == sf_mem_end()) init = 1; //Whether this is the first page allocated, to know if we have to make prologue
    void *s = sf_mem_grow(); //Allocated page of memory
    //refreshpal(sf_mem_start());
    if(init) { //Make prologue
        fprintf(stderr,"INIT!\n");
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

        findadd(bigfree); //Add block to free list 9
    }
    else{
        sf_block *epilogp = (sf_block *)(s-8);
        sf_block *previsfree = prevfree(epilogp);
        fprintf(stderr,"Prev pointer? %p\n",previsfree);
        if(previsfree) { //If previous block is free, must coalesce it with new page of memory!
            delfl(previsfree);
            previsfree->header+=8;
            sf_footer *lfoot = getfooter(previsfree);
            *lfoot = (sf_footer)previsfree->header;
            findadd(previsfree);
            
            sf_block *epipoint = (sf_block *)(s);
            fprintf(stderr,"FOOTER VS NEXT HEADER: %p %p %ld\n",lfoot,epipoint,(long int)epipoint-(long int)lfoot);
            sf_block bigfree;
            sf_header bighead = (sf_header)(PAGE_SZ-8);
            bigfree.header = bighead;
            *epipoint = bigfree;
            sf_footer *bigfoot = getfooter(epipoint);
            *bigfoot = (sf_footer)epipoint->header;
            findadd(epipoint);
            //sf_show_heap();
            coalesce(previsfree);
        }
        else { //Else, just make head of new memory the epilogue of last page!
            sf_block *epipoint = (sf_block *)(s-8);
            sf_block bigfree;
            sf_header bighead = (sf_header)(PAGE_SZ);
            bigfree.header = bighead;
            *epipoint = bigfree;
            sf_footer *bigfoot = getfooter(epipoint);
            *bigfoot = (sf_footer)epipoint->header;
            findadd(epipoint);
        }
    }
    //Make new epilogue.
    sf_block *end = (sf_mem_end()-sizeof(sf_header)); //Where epilogue will be
    sf_block epilog;
    sf_header epih = 0 | THIS_BLOCK_ALLOCATED;
    epilog.header = epih;
    *end=epilog; //Sets epilogue
    sf_show_heap();
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
        fprintf(stderr,"SEEN SIZE %ld in %d!\n",(this->header/8 << 3),i);
        if((this->header/8 << 3) >= size) {
            return this;
        }
        this = this->body.links.next;
    }
    fprintf(stderr,"GETS HERE FOR SEARCH OF SIZE %ld, IN LIST %d\n",size,i);
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
    size_t fullsize = size%8 == 0 ? size + 8 : (size + 8)+(8-size%8); //Round to multiple of 8, fullsize is size with header
    if(fullsize < 32) fullsize = 32;
    if(sf_mem_start() == sf_mem_end()) { //If total mem pages is 0, mem grow
        newmem();
        totalsz+=PAGE_SZ;
    }

    //Check if it is valid for quick lists
    //void *g = (void *)0;
    if(fullsize >= MIN_BLOCK_SIZE && fullsize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8) {
        //int qind = (fullsize - MIN_BLOCK_SIZE)/8;
        //fprintf(stderr,"qind: %d\n",qind);
        //g = addql(qind);
        //fprintf(stderr,"after\n");
    }

    void *g = checkfree(fullsize);
    fprintf(stderr,"FREE POINT FOR %ld? %p\n",fullsize,g);
    if(g != NULL) {
        g = malsplit(g,fullsize);
    }
    else{
        newmem();
        totalsz+=PAGE_SZ;
        fprintf(stderr,"CALLING MALLOC AGAIn\n");
        g = sf_malloc(size);
    }
    refreshpal(sf_mem_start());
    //sf_show_heap();
    return g;
}

void sf_free(void *pp) {
    if(pp == NULL) abort();
    sf_block *p = (sf_block *)pp;
    sf_header head = p->header;
    size_t psize = (head/8 << 3); //size of block pointed to by pp
    if((long int)p % 8 || psize % 8 || psize < 32) abort();
    if(!(head&THIS_BLOCK_ALLOCATED)||(head&IN_QUICK_LIST)) abort();
    if(pp < sf_mem_start() || getfooter(p) > sf_mem_end()) abort();
    //Also check for if prev alloc is 1 and preceding block alloc is 0
    sf_block freeb;
    head-=THIS_BLOCK_ALLOCATED;
    freeb.header = head;
    sf_footer *footer = (sf_footer *)getfooter(p);
    *p = freeb;
    *footer = (sf_footer)(p->header);
    findadd(p);
    coalesce(p);
    refreshpal(sf_mem_start());

}

void *sf_realloc(void *pp, size_t rsize) {
    // TO BE IMPLEMENTED
    abort();
}

void *sf_memalign(size_t size, size_t align) {
    // TO BE IMPLEMENTED
    abort();
}
