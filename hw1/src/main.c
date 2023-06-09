#include <stdio.h>
#include <stdlib.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

int main(int argc, char **argv)
{
    if(validargs(argc, argv))
       USAGE(*argv, EXIT_FAILURE);
    if(global_options == HELP_OPTION)
       USAGE(*argv, EXIT_SUCCESS);
    FILE* output = stdout;
    FILE* diff_file = fopen(diff_filename,"r");
    int status;
    if(diff_file) {
        status = patch(stdin,output,diff_file);
    }
    else {
        if((global_options & QUIET_OPTION) != QUIET_OPTION) {
            fprintf(stderr,"File Not Found.\n");
        }
        return EXIT_FAILURE;
    }
    fclose(diff_file);
    return status == -1 ? EXIT_FAILURE : EXIT_SUCCESS; 
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
