#include <stdlib.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the various options that were specified will be
 * encoded in the global variable 'global_options', where it will be
 * accessible elsewhere in the program.  For details of the required
 * encoding, see the assignment handout.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain an encoded representation
 * of the selected program options.
 * @modifies global variable "diff_filename" to point to the name of the file
 * containing the diffs to be used.
 */

int validargs(int argc, char **argv) {
    global_options=0x0;
    if(argc <= 1) return -1;
    char** currarg = argv;
    currarg++;
    for(int argcounter = argc-1; argcounter > 0; argcounter--) {
        //printf("%d, %s",argcounter,*currarg);
        int argcount = argc - argcounter;
        int charcount = 0;
        int isflag = 0;
        for(char* cchar = *currarg; *cchar != '\0'; cchar++) {
            if(charcount == 0) {
                if(*cchar == '-'){
                    isflag = 1;
                }
            }
            else if(charcount == 1) {
                if(*(cchar-1) == '-'){
                    if(*(cchar+1) == '\0') {
                        if(*cchar == 'h') {
                            printf("Help Win");
                            global_options=0x1;
                            return 0;
                        }
                        else if(*cchar == 'n') {
                            printf("N");
                            if((global_options & 0x2) != 0x0) {
                                global_options = 0x0;
                                printf("N FAIL");
                                return -1;
                            }
                            global_options=global_options | 0x2;
                            printf("N PASS");
                        }
                        else if(*cchar == 'q') {
                            printf("Q");
                            if((global_options & 0x4) != 0x0) {
                                global_options = 0x0;
                                return -1;
                            }
                            global_options=global_options | 0x4;
                        }
                        else{
                            printf(" G FAIL");
                            global_options=0x0;
                            return -1;
                        }
                        if(argcount == argc-1) {
                            printf("INVALID");
                            global_options=0x0;
                            return -1;
                        }
                    }
                    else {
                        printf("Help fail");
                        global_options=0x0;
                        return -1;
                    }
                }
            }
            else if(argcount != argc-1) {
                printf("INVALID2");
                global_options=0x0;
                return -1;
            }
            else {
                if(isflag == 1) {
                    global_options=0x0;
                    return -1;
                }
                //printf("FOUND LAST");
            }
            printf("%c", *cchar);
            charcount++;
        }
        if(argcount == argc-1) {
            if(isflag == 1) {
                global_options=0x0;
                return -1;
            }
            printf("Filename: %s",*currarg);
            diff_filename  = *currarg;
            return 0;
        }
        else if(isflag == 0) {
            global_options=0x0;
            return -1;
        }
        currarg++;
    }
    return 0;
    abort();
}
