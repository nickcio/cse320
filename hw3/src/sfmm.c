/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "helper.h"

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
    for(size_t min = MIN_BLOCK_SIZE; min < psize && i < NUM_FREE_LISTS-1; min*=2) i++; //Find correct list
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

sf_footer *getprev(sf_block *s) {//Gets footer before s
    if((void *)s > sf_mem_start()+8) return (sf_footer *)((void *)s - 8);
    return NULL;
}

void refreshpal(sf_block *s) {//s is start of the entire heap. refreshes all prev alloc flags.
    sf_block *curr = getnextblock(s);
    size_t ssize = (s->header/8 << 3);
    int al = 0;
    int pal = 1;
    while(ssize) {
        sf_header head = curr->header;
        ssize = (head/8 << 3);

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
    if((next->header&IN_QUICK_LIST)) return 0;
    if(!((next->header)&THIS_BLOCK_ALLOCATED)) {
        return nextsize;
    }
    return 0;
}

sf_block *prevfree(sf_block *s) {//Finds if prev block is free in the heap, returns pointer
    if((s->header&PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED) return NULL;
    sf_header *head = (sf_header *)getheader((void *)(s)-8);
    if((*head&THIS_BLOCK_ALLOCATED) == 0) return (sf_block *)head;
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
    if(!nextsize && !prevpoint) return;
    delfl(s);
    if(nextsize) {
        delfl(getnextblock(s));
    }
    if(prevpoint) {
        delfl(prevpoint);
        delfree(s);
        s = prevpoint;
        nextsize+=currsize;
    }

    sf_block newblock;
    sf_header newhead = (sf_header)((s->header)+nextsize);
    newblock.header = newhead;
    *s = newblock;
    sf_footer *footer = (sf_footer*)getfooter(s);

    *footer = (sf_footer)newhead;
    findadd(s);

}

int newmem() { //Memgrow + coalescing
    int init = 0;
    if(sf_mem_start() == sf_mem_end()) init = 1; //Whether this is the first page allocated, to know if we have to make prologue
    void *s = sf_mem_grow(); //Allocated page of memory
    if(s == NULL) return 0;
    if(init) { //Make prologue

        sf_block prolog; //Prologue
        prolog.header = (sf_header)(32 | THIS_BLOCK_ALLOCATED); //Header for 32 byte block, alloc flag == 1
        *((sf_block*)s) = prolog;
        //sf_show_block(s);

        sf_block *bigfree = s+32; //Pointer to first free block
        bigfree->header = (sf_header)((PAGE_SZ-MIN_BLOCK_SIZE-8) | PREV_BLOCK_ALLOCATED); //Sets big sized free block, alloc flag = 0
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
        if(previsfree) { //If previous block is free, must coalesce it with new page of memory!
            delfl(previsfree);
            previsfree->header+=8;
            sf_footer *lfoot = getfooter(previsfree);
            *lfoot = (sf_footer)previsfree->header;
            findadd(previsfree);
            
            sf_block *epipoint = (sf_block *)(s);
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
    sf_block *end = (sf_block *)(sf_mem_end()-8); //Where epilogue will be
    end->header = 0 | THIS_BLOCK_ALLOCATED;
    //sf_show_heap();
    return 1;
}

void flushql(int i) { //flushes quicklist i
    sf_block *head = sf_quick_lists[i].first;
    while(head) {
        sf_block *curr = head;
        curr->header-=(curr->header&IN_QUICK_LIST);
        curr->header-=(curr->header&THIS_BLOCK_ALLOCATED);
        head = head->body.links.next;
        findadd(curr);
        refreshpal(sf_mem_start());
        coalesce(curr);
    }
    sf_quick_lists[i].first=NULL;
    sf_quick_lists[i].length = 0;
}

void *addql(sf_block *b, int index) { //Assumes index is valid
    int len = sf_quick_lists[index].length;
    if(len == 0) sf_quick_lists[index].first = NULL;
    if(len == QUICK_LIST_MAX) {
        flushql(index);
    }
    b->body.links.next = sf_quick_lists[index].first;
    b->body.links.prev = NULL;
    sf_quick_lists[index].first = b;
    b->header|=IN_QUICK_LIST;
    b->header|=THIS_BLOCK_ALLOCATED;
    sf_quick_lists[index].length+=1;

    return b;
}

void *findaddql(sf_block *p) { //Find where to add a block to a quicklist, returns NULL if it can't be added to one
    size_t psize = (p->header/8 << 3);
    if(psize >= MIN_BLOCK_SIZE && psize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8) {
        int i = (psize - MIN_BLOCK_SIZE)/8;
        return addql(p,i);
    }
    return NULL;
}

void *malql(int i) {//Allocates a free block from a quick list i
    if(i < 0 || i > QUICK_LIST_MAX-1) return NULL;
    if(sf_quick_lists[i].length < 1) return NULL;
    sf_block *b = sf_quick_lists[i].first;
    sf_quick_lists[i].first = b->body.links.next;
    sf_quick_lists[i].length-=1;
    b->header-=(b->header&IN_QUICK_LIST);
    b->header|=THIS_BLOCK_ALLOCATED;
    return b;
}

void *memalsplit(sf_block *f, sf_block *p, size_t sizep) {//f is free block before p, p is specific block to be allocated
    size_t fullsize = (f->header/8 << 3);
    delfl(f);
    size_t sizef = (size_t)p - (size_t)f;
    size_t sizelast = fullsize - sizef - sizep;
    if(sizelast < 32) { //Will leave splinter on the right side, so allocate bigger block!
        sizep+=sizelast;
        sizelast = 0;
    }
    sf_block fblock; //Free block before p
    fblock.header = (sf_header)(sizef|(f->header&PREV_BLOCK_ALLOCATED));
    *f = fblock;
    sf_footer *ffoot = (sf_footer *)getfooter(f);
    *ffoot = fblock.header;
    findadd(f);

    sf_block pblock; //Memaligned block
    pblock.header = (sf_header)(sizep|THIS_BLOCK_ALLOCATED);
    *p = pblock;

    if(sizelast) { //Free block after p
        sf_block *l = getnextblock(p);
        sf_block lblock;
        lblock.header = (sf_header)(sizelast|PREV_BLOCK_ALLOCATED);
        *l = lblock;
        sf_footer *lfoot = (sf_footer *)getfooter(l);
        *lfoot = lblock.header;
        findadd(l);
    }

    return p;
}

void *realsplit(sf_block *p, size_t size) { //Reallocates part of an allocated block, assumes size < size of block pointed to by p
    size_t psize = (p->header/8 << 3); //size of block pointed to by p

    if(psize - size < 32) { //Splitting the block will result in splintering, so just leave the splinter
        return p;
    }
    sf_block *next = getnextblock(p);
    size_t qsize = (psize - size) >> 3 << 3;
    sf_block *newfree = (sf_block *)((void *)p+size);
    sf_block freeb;
    freeb.header = (sf_header)(qsize | PREV_BLOCK_ALLOCATED);
    *newfree = freeb;
    sf_footer *freefooter = (sf_footer *)getfooter(newfree);
    *freefooter = freeb.header;
    findadd(newfree);
    next->header-=(next->header&PREV_BLOCK_ALLOCATED);
    if(!(next->header&THIS_BLOCK_ALLOCATED)) {
        sf_footer *nextfooter = (sf_footer *)getfooter(next);
        *nextfooter = next->header;
        coalesce(next);
    }
    //coalesce(newfree);

    p->header = (sf_header)((size) | THIS_BLOCK_ALLOCATED | (p->header&PREV_BLOCK_ALLOCATED));
    return p;
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
    freeb.header = (sf_header)(qsize | PREV_BLOCK_ALLOCATED);
    *newfree = freeb;
    sf_footer *freefooter = (sf_footer *)getfooter(newfree);
    *freefooter = (sf_footer)freeb.header;
    findadd(newfree);
    //coalesce(newfree);

    sf_block newb;
    newb.header = (sf_header)((size) | THIS_BLOCK_ALLOCATED);
    *p = newb;
    return p;
}

void *findfree(size_t size, int i) { //Attempts to find a free block of at least size in free list i
    sf_block *head = &sf_free_list_heads[i];
    sf_block *this = head->body.links.next;
    while(this != head) {
        if((this->header/8 << 3) >= size) {
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
    size_t fullsize = size%8 == 0 ? size + 8 : (size + 8)+(8-size%8); //Round to multiple of 8, fullsize is size with header
    if(fullsize < 32) fullsize = 32;
    if(sf_mem_start() == sf_mem_end()) { //If total mem pages is 0, mem grow
        int mem = newmem();
        if(!mem) { //NO MEMORY HAS BEEN ALLOCATED
            sf_errno = ENOMEM;
            return NULL;
        }
    }

    //Check if it is valid for quick lists
    void *g = NULL;
    if(fullsize >= MIN_BLOCK_SIZE && fullsize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8 && fullsize%8==0) {
        g = malql((fullsize-MIN_BLOCK_SIZE)>>3);
    }
    if(g == NULL) {
        g = checkfree(fullsize); //Check if a block of fullsize is free
    }
    if(g != NULL) { //If there is one
        g = malsplit(g,fullsize);
    }
    else{ //Allocate new page and attempt to malloc again
        if(sf_errno == ENOMEM) return NULL;
        int mem = newmem();
        if(!mem) { //NO MEMORY HAS BEEN ALLOCATED
            sf_errno = ENOMEM;
            return NULL;
        }
        g = sf_malloc(size);
        if(g) g-=8;
        else return NULL;
    }

    //refreshpal(sf_mem_start()); //Remove this and the code breaks!
    //Set next pal bit
    sf_block *next = getnextblock(g);
    if(next != sf_mem_end() - 8) next->header|=PREV_BLOCK_ALLOCATED;
    //Remove ql bit
    next->header-=(next->header&IN_QUICK_LIST);

    return g+8;
}

void sf_free(void *pp) {
    //A bunch of checks for invalid pointers as described by the doc
    if(pp == NULL) abort();
    pp-=8;
    sf_block *p = (sf_block *)pp;
    sf_header head = p->header;
    size_t psize = (head/8 << 3); //size of block pointed to by pp
    if((long unsigned int)p % 8 || psize % 8 || psize < 32) abort();
    if(!(head&THIS_BLOCK_ALLOCATED)||(head&IN_QUICK_LIST)) abort();
    if(pp < sf_mem_start() || getfooter(p) > sf_mem_end()) abort();
    //TODO: Also check for if prev alloc is 1 and preceding block alloc is 0
    sf_footer *prev = getprev(p);
    if(prev != NULL) if(!(p->header&PREV_BLOCK_ALLOCATED)&&(*prev&THIS_BLOCK_ALLOCATED)) abort();

    int qvalid = (psize >= MIN_BLOCK_SIZE && psize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8); //Whether this block can be sent to quick list or not
    //Make the freed block
    if(!qvalid) { //Sent to free list
        sf_block freeb;
        head-=THIS_BLOCK_ALLOCATED;
        freeb.header = head;
        sf_footer *footer = (sf_footer *)getfooter(p);
        *p = freeb;
        *footer = (sf_footer)(p->header);
        findadd(p);
        refreshpal(sf_mem_start()); //Don't remove this one either!
        coalesce(p);
    }
    else { //Sent to quick list
        sf_block freeb;
        head-=THIS_BLOCK_ALLOCATED;
        freeb.header = head|IN_QUICK_LIST;
        *p = freeb;
        findaddql(p);
        refreshpal(sf_mem_start());
    }
    //Set next pal bit
    if(!(p->header&THIS_BLOCK_ALLOCATED)) {
        sf_block *next = getnextblock(p);
        if(next != sf_mem_end() - 8) {
            next->header-=(next->header&PREV_BLOCK_ALLOCATED);
        }
    }

}

void *sf_realloc(void *pp, size_t rsize) {
    if(rsize == 0) return NULL;
    size_t fullsize = rsize%8 == 0 ? rsize + 8 : (rsize + 8)+(8-rsize%8);
    if(fullsize < 32) fullsize = 32;
     //A bunch of checks for invalid pointers as described by the doc
    if(pp == NULL) {
        sf_errno = EINVAL;
        return NULL;
    }
    pp-=8;
    sf_block *p = (sf_block *)pp;
    sf_header head = p->header;
    size_t psize = (head/8 << 3); //size of block pointed to by pp
    if((long unsigned int)p % 8 || psize % 8 || psize < 32) {
        sf_errno = EINVAL;
        return NULL;
    }
    if(!(head&THIS_BLOCK_ALLOCATED)||(head&IN_QUICK_LIST)) {
        sf_errno = EINVAL;
        return NULL;
    }
    if(pp < sf_mem_start() || getfooter(p) > sf_mem_end()) {
        sf_errno = EINVAL;
        return NULL;
    }
    sf_footer *prev = getprev(p);
    if(prev != NULL) if(!(p->header&PREV_BLOCK_ALLOCATED)&&(*prev&THIS_BLOCK_ALLOCATED)) {
        sf_errno = EINVAL;
        return NULL;
    }

    pp+=8;
    if(rsize > psize) {
        sf_block *newb = sf_malloc(rsize);
        if(newb != NULL) {
            memcpy(newb,pp,psize-8);
            sf_free(pp);
            return newb;
        }
        return NULL;
    }
    void *newb = realsplit(pp-8,fullsize);
    //newb->header|=THIS_BLOCK_ALLOCATED;
    refreshpal(sf_mem_start());
    return newb+8;
}

void *sf_memalign(size_t size, size_t align) {
    if(size == 0) return NULL;
    int aligned = 1; //Check if valid align size
    for(size_t power = 8; power <= align; power*=2) if(align % power != 0) aligned = 0;
    if(align < 8 || !aligned) {
        sf_errno = EINVAL;
        return NULL;
    }

    if(sf_mem_start() == sf_mem_end()) { //If total mem pages is 0, mem grow
        int mem = newmem();
        if(!mem) { //NO MEMORY HAS BEEN ALLOCATED
            sf_errno = ENOMEM;
            return NULL;
        }
    }
    size_t reqsize = size%8==0 ? size+align+MIN_BLOCK_SIZE+8 : size+align+MIN_BLOCK_SIZE+8+(8-size%8);
    size_t fullsize = size%8==0 ? size+8 : size+8+(8-size%8);
    void *g = NULL;
    if(fullsize >= MIN_BLOCK_SIZE && fullsize <= MIN_BLOCK_SIZE+(NUM_QUICK_LISTS-1)*8 && fullsize%8==0) {
        g = malql((fullsize-MIN_BLOCK_SIZE)>>3);
    }
    if(g == NULL) {
        g = checkfree(reqsize);
    }
    if(g != NULL) {
        if((long unsigned int)(g+8)%align == 0) {
            g = malsplit(g,fullsize);
        }
        else{
            void *p = (long unsigned int)(g+MIN_BLOCK_SIZE+8)%align == 0 ? (g+MIN_BLOCK_SIZE) : (void *)((g+MIN_BLOCK_SIZE) + (align - ((long int)g+MIN_BLOCK_SIZE+8)%align));
            g = memalsplit(g,p,fullsize);
        }
        refreshpal(sf_mem_start());
    }
    else{ //Allocate new page and attempt to memalign again
        if(sf_errno == ENOMEM) return NULL;
        int mem = newmem();
        if(!mem) { //NO MEMORY HAS BEEN ALLOCATED
            sf_errno = ENOMEM;
            return NULL;
        }
        g = sf_memalign(size,align);
        if(g) g-=8;
        else return NULL;
    }
    
    return g+8;
}
