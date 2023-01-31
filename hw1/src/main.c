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
    if((global_options & NO_PATCH_OPTION) == NO_PATCH_OPTION) {
        fclose(stdout);
        output = stderr;
    }
    FILE* diff_file = fopen(diff_filename,"r");
    patch(stdin,output,diff_file);

    return EXIT_FAILURE; 
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
