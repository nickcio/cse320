#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    /* double* ptr = sf_malloc(sizeof(double)*3);
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

    sf_malloc(3480-8);
    sf_malloc(3440);

    sf_show_heap();

    sf_malloc(400);

    sf_show_heap(); */

    void *x = sf_malloc(86100);
    fprintf(stderr,"Done.\n");
    fprintf(stderr,"Hi %p\n",x);
    sf_show_heap();


    return EXIT_SUCCESS;
}
