#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & ~0x7))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

/*
 * Assert that the free list with a specified index has the specified number of
 * blocks in it.
 */
void assert_free_list_size(int index, int size) {
    int cnt = 0;
    sf_block *bp = sf_free_list_heads[index].body.links.next;
    while(bp != &sf_free_list_heads[index]) {
	cnt++;
	bp = bp->body.links.next;
    }
    cr_assert_eq(cnt, size, "Free list %d has wrong number of free blocks (exp=%d, found=%d)",
		 index, size, cnt);
}

/*
 * Assert the total number of quick list blocks of a specified size.
 * If size == 0, then assert the total number of all quick list blocks.
 */
void assert_quick_list_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_QUICK_LISTS; i++) {
	sf_block *bp = sf_quick_lists[i].first;
	while(bp != NULL) {
	    if(size == 0 || size == (bp->header & ~0x7))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of quick list blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of quick list blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz = sizeof(int);
	int *x = sf_malloc(sz);

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(4024, 1);
	assert_free_list_size(7, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	// We want to allocate up to exactly four pages, so there has to be space
	// for the header and the link pointers.
	void *x = sf_malloc(16336);
	cr_assert_not_null(x, "x is NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(86100);

	cr_assert_null(x, "x is not NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(85976, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_quick, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 32, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(40, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(3952, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(208, 1);
	assert_free_block_count(3784, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	sf_free(x);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(520, 1);
	assert_free_block_count(3472, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
        size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200, sz_z = 700;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 4);
	assert_free_block_count(208, 3);
	assert_free_block_count(1896, 1);
	assert_free_list_size(3, 3);
	assert_free_list_size(6, 1);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x7) == 88, "Realloc'ed block size not what was expected!");

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(32, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(3904, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char *)y - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x7) == 88, "Realloc'ed block size not what was expected!");

	// There should be only one free block.
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(3968, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)y - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x7) == 32, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size ADJUSTED_BLOCK_SIZE(sz_x) - ADJUSTED_BLOCK_SIZE(sz_y)
	// to the freelist.  This block will go into the main freelist and be coalesced.
	// Note that we don't put split blocks into the quick lists because their sizes are not sizes
	// that were requested by the client, so they are not very likely to satisfy a new request.
	assert_quick_list_block_count(0, 0);	
	assert_free_block_count(0, 1);
	assert_free_block_count(4024, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

//Test(sfmm_student_suite, student_test_1, .timeout = TEST_TIMEOUT) {
//}

Test(sfmm_student_suite, student_test_1, .timeout = TEST_TIMEOUT) {
	//Makes sure x is aligned, and makes sure memalign returns NULL and sets sf_errno for y
	size_t sz_x = 400;
	size_t align_x = 64;
	void *x = sf_memalign(sz_x,align_x);

	cr_assert_not_null(x, "x is NULL!");

	sf_block *bp = (sf_block *)((char *)x - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((long unsigned int)x%align_x == 0, "Payload alignment not what was expected!");

	size_t sz_y = 640;
	size_t align_y = 27;
	void *y = sf_memalign(sz_y,align_y);
	cr_assert(sf_errno == EINVAL, "Expected sf_errno to be EINVAL!");
	cr_assert_null(y, "y is not NULL!");	
}

Test(sfmm_student_suite, student_test_2, .timeout = TEST_TIMEOUT) {
	//Makes sure malloc and memalign (alignment 8) give different pointers, because x goes to quick lists after being freed!
	//memalign should act identical to malloc when given alignment 8, so there should only be 1 free block at the end.
	size_t sz_x = sizeof(double)*5;
	size_t align_x = 8;
	void *x = sf_memalign(sz_x,align_x);
	sf_free(x);
	size_t sz_y = sizeof(double)*3;
	void *y = sf_malloc(sz_y);

	cr_assert_not_null(x, "x is NULL!");
	cr_assert_not_null(y, "y is NULL!");
	sf_show_heap();
	sf_block *bp = (sf_block *)((char *)y - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(x != y, "Pointers are the same!");
	assert_quick_list_block_count(48,1);
	assert_free_block_count(0,1);
	assert_free_block_count(3976,1);
}

Test(sfmm_student_suite, student_test_3, .timeout = TEST_TIMEOUT) {
	//Testing lots of mallocs and frees and reallocs!
	double* ptr0 = sf_malloc(sizeof(double)*3);
    double* ptr1 = sf_malloc(sizeof(double)*6);
    double* ptr2 = sf_malloc(sizeof(double)*6);
    double* ptr3 = sf_malloc(sizeof(double)*3);
    double* ptr4 = sf_malloc(sizeof(double)*6);
    double* ptr5 = sf_malloc(sizeof(double)*6);
    double* ptr6 = sf_malloc(3400);
    double* ptr7 = sf_malloc(1000);

	cr_assert_not_null(ptr0, "ptr0 is NULL!");
	cr_assert_not_null(ptr1, "ptr1 is NULL!");
	cr_assert_not_null(ptr2, "ptr2 is NULL!");
	cr_assert_not_null(ptr3, "ptr3 is NULL!");
	cr_assert_not_null(ptr4, "ptr4 is NULL!");
	cr_assert_not_null(ptr5, "ptr5 is NULL!");
	cr_assert_not_null(ptr6, "ptr6 is NULL!");
	cr_assert_not_null(ptr7, "ptr7 is NULL!");

    sf_free(ptr0);
	sf_free(ptr3);
    sf_free(ptr1);
	sf_free(ptr5);
    sf_free(ptr2);
    sf_free(ptr6);

	sf_realloc(ptr4,sizeof(double)*15);
	sf_realloc(ptr7,500);

	assert_quick_list_block_count(32,2);
	assert_quick_list_block_count(56,4);
	assert_free_block_count(0,2);
	assert_free_block_count(3280,1);
	assert_free_block_count(3944,1);
}

Test(sfmm_student_suite, student_test_4, .timeout = TEST_TIMEOUT) {
	//Tests the invalid input capabilities of realloc!
	sf_errno = 0;
	void *p1 = sf_malloc(sizeof(double)*32);
	cr_assert_not_null(p1, "p1 is NULL!");
	//Realloc to 0 size should return NULL and not set sf_errno!
	void *p1n = sf_realloc(p1,0);
	cr_assert(sf_errno == 0, "Expected sf_errno to be 0!");
	cr_assert_null(p1n, "p1n is not NULL!");
	//Realloc to null pointer should return NULL and set sf_errno to EINVAL!
	p1n = sf_realloc(p1n,sizeof(double)*40);
	cr_assert(sf_errno == EINVAL, "Expected sf_errno to be EINVAL!");
	cr_assert_null(p1n, "p1n is not NULL!");

	sf_block *bp = (sf_block *)((char *)p1 - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x7) == 264, "Block size not what was expected!");

	assert_free_block_count(0,1);
	assert_free_block_count(3792,1);

	//Now using original pointer to realloc to make sure the data wasn't changed!
	p1 = sf_realloc(p1,sizeof(double)*40);
	cr_assert_not_null(p1, "p1 is NULL!");

	sf_block *bp2 = (sf_block *)((char *)p1 - sizeof(sf_header));
	cr_assert(bp2->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp2->header & ~0x7) == 328, "Block size not what was expected!");

	assert_free_block_count(0,2);
	assert_free_block_count(264,1);
	assert_free_block_count(3464,1);
}

Test(sfmm_student_suite, student_test_5, .timeout = TEST_TIMEOUT) {
	//Stress testing memalign, make sure every block is aligned correctly! Also makes sure memalign allocates memory when needed!
	void *p = sf_memalign(846,64);
	sf_block *bp1 = (sf_block *)((char *)p - sizeof(sf_header));
	cr_assert(bp1->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((long unsigned int)p%64 == 0, "Block p Payload alignment not what was expected!");

    void *q = sf_memalign(938,128);
    sf_block *bp2 = (sf_block *)((char *)q - sizeof(sf_header));
	cr_assert(bp2->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((long unsigned int)q%128 == 0, "Block q Payload alignment not what was expected!");

    void *r = sf_memalign(50,8);
    sf_block *bp3 = (sf_block *)((char *)r - sizeof(sf_header));
	cr_assert(bp3->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((long unsigned int)r%8 == 0, "Block r Payload alignment not what was expected!");

    void *s = sf_memalign(4000,16);
    sf_block *bp4 = (sf_block *)((char *)s - sizeof(sf_header));
	cr_assert(bp4->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((long unsigned int)s%16 == 0, "Block s Payload alignment not what was expected!");

    void *t = sf_memalign(700,32);
    sf_block *bp5 = (sf_block *)((char *)t - sizeof(sf_header));
	cr_assert(bp5->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((long unsigned int)t%32 == 0, "Block t Payload alignment not what was expected!");
}