#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    double* ptr = sf_malloc(sizeof(double)*3);
    double* ptr2 = sf_malloc(sizeof(double)*5);
    double* ptr3 = sf_malloc(sizeof(double)*6);
    double* ptr4 = sf_malloc(sizeof(double)*9);
    double* ptr5 = sf_malloc(sizeof(double)*4);
    printf("%p %p %p %p %p\n", ptr,ptr2,ptr3,ptr4,ptr5);

    sf_free(ptr);
    sf_free(ptr3);

    sf_show_heap();

    return EXIT_SUCCESS;
}
