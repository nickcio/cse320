#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    double* ptr = sf_malloc(sizeof(double)*4);
    fprintf(stderr,"HERE\n");
    printf("%f\n", *ptr);
    fprintf(stderr,"HERE\n");
    sf_free(ptr);

    return EXIT_SUCCESS;
}
