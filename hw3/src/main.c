#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    double* ptr = sf_malloc(sizeof(double)*3);
    double* ptrg = sf_malloc(sizeof(double)*3);
    double* ptr2 = sf_malloc(sizeof(double)*5);
    double* ptr3 = sf_malloc(sizeof(double)*6);
    double* ptr4 = sf_malloc(sizeof(double)*9);
    double* ptr5 = sf_malloc(sizeof(double)*4);
    double* ptr6 = sf_malloc(3400);
    double* ptr7 = sf_malloc(1000);
    printf("%p %p %p %p %p %p %p\n", ptr,ptr2,ptr3,ptr4,ptr5,ptr6,ptr7);

    sf_free(ptr);
    sf_free(ptrg);
    sf_free(ptr2);

    sf_show_heap();

    sf_free(ptr3);
    sf_free(ptr5);

    sf_show_heap();

    sf_free(ptr6);

    sf_show_heap();

    void *p = sf_memalign(846,64);
    fprintf(stderr,"pointer mod 64? %p %ld\n",p-8,(long int)p%64);

    void *q = sf_memalign(938,128);
    fprintf(stderr,"pointer mod 128? %p %ld\n",q-8,(long int)q%128);

    void *r = sf_memalign(50,8);
    fprintf(stderr,"pointer mod 8? %p %ld\n",r-8,(long int)r%8);

    void *s = sf_memalign(4000,64);
    fprintf(stderr,"pointer mod 64? %p %ld\n",s-8,(long int)s%64);

    void *t = sf_memalign(700,32);
    fprintf(stderr,"pointer mod 32? %p %ld\n",t-8,(long int)t%32);
    sf_show_heap();
    sf_free(q);
    sf_show_heap();
    
    return EXIT_SUCCESS;
}
