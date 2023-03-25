#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    void* p = sf_malloc(100);
    sf_show_heap();
    p = sf_realloc(p,108);
    sf_show_heap();
    p = sf_realloc(p,106);
    sf_show_heap();
    p = sf_realloc(p,50);
    sf_show_heap();
    
    return EXIT_SUCCESS;
}
