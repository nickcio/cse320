#include <stdlib.h>
#include <stdio.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

/**
 * @brief Get the header of the next hunk in a diff file.
 * @details This function advances to the beginning of the next hunk
 * in a diff file, reads and parses the header line of the hunk,
 * and initializes a HUNK structure with the result.
 *
 * @param hp  Pointer to a HUNK structure provided by the caller.
 * Information about the next hunk will be stored in this structure.
 * @param in  Input stream from which hunks are being read.
 * @return 0  if the next hunk was successfully located and parsed,
 * EOF if end-of-file was encountered or there was an error reading
 * from the input stream, or ERR if the data in the input stream
 * could not be properly interpreted as a hunk.
 */
static int line_start = 1;
static int serial = 0;

int hunk_next(HUNK *hp, FILE *in) {
    FILE *temp = in;
    char curr;
    int index = 0;

    int in_num = 0;
    int type = 0;
    int firstin1 = -1;
    int lastin1 = -1;
    int firstin2 = -1;
    int lastin2 = -1;
    //fprintf(stderr,"Start");
    while(curr != EOF){
        //fprintf(stderr,"MILESTONE");
        //fprintf(stderr,"%c",curr);
        curr = fgetc(temp);
        if(line_start || in_num) {
            int num = curr-48;
            //fprintf(stderr,"MILESTONE2 %c %d", curr,curr);
            if(num >= 0 && num <= 9) {
                in_num = 1;
                index *= 10;
                index += num;
                //fprintf(stderr,"MILESTONE3");
                //fprintf(stderr,"Num: %d, Index: %d .",num,index);
            }
            else if(curr == ',' && in_num) {
                if(firstin1 == -1) {
                    firstin1 = index;
                    //fprintf(stderr,"Firstin1: %d ", firstin1);
                }
                else if(firstin2 == -1) {
                    firstin2 = index;
                    //fprintf(stderr,"Firstin2: %d ", firstin2);
                }
                index = 0;
            }
            else if((curr == 'a' || curr == 'd' || curr == 'c') && in_num) {
                type = curr == 'a' ? 1 : curr == 'd' ? 2 : curr == 'c' ? 3 : 0;
                if(lastin1 == -1) {
                    lastin1 = index;
                    firstin1 = firstin1 == -1 ? lastin1 : firstin1;
                    //fprintf(stderr,"Firstin1: %d ", firstin1);
                }
                else if(lastin1 != -1 || firstin2 != -1 || lastin2 != -1) {
                    in_num = 0;
                    return ERR;
                }
                index = 0;
            }
            else if(curr == '\n' && in_num) {
                if(lastin2 == -1) {
                    lastin2 = index;
                    firstin2 = firstin2 == -1 ? lastin2 : firstin2;
                }
                if(lastin1 == -1 || firstin1 == -1) {
                    in_num = 0;
                    return ERR;
                }
                else{
                    if((type == 1 && firstin1 != lastin1) || (type == 2 && firstin2 != lastin2) || lastin1 < firstin1 || lastin2 < firstin2) {
                        return ERR;
                    }
                    else {
                        serial++;
                        (*hp).serial = serial;
                        (*hp).old_start = firstin1;
                        (*hp).old_end = lastin1 >= 0 ? lastin1 : firstin1;
                        (*hp).new_start = firstin2;
                        (*hp).new_end = lastin2 >= 0 ? lastin2 : firstin2;
                        (*hp).type = type == 1 ? HUNK_APPEND_TYPE : type == 2 ? HUNK_DELETE_TYPE : type == 3 ? HUNK_CHANGE_TYPE: HUNK_NO_TYPE;
                        return 0;
                    }
                }
                line_start = 1;
                in_num = 0;
                type = 0;
                firstin1 = -1;
                lastin1 = -1;
                firstin2 = -1;
                lastin2 = -1;
                index = 0;
            }
            else if(curr == EOF && in_num) {
                //fprintf(stderr,"DONE");
                return ERR;
            }
            else{
                line_start = 0;
                if(in_num == 0) {
                    if(curr == '-') {
                        if(fgetc(temp) == '-') {
                            if(fgetc(temp) == '-') {
                                if(fgetc(temp) == '\n'){
                                    line_start = 1;
                                }
                                else{
                                    //fprintf(stderr,"E 1");
                                    return ERR;
                                }
                            }
                            else{
                                //fprintf(stderr,"E 1");
                                return ERR;
                            }
                        }
                        else{
                            //fprintf(stderr,"E 1");
                            return ERR;
                        }
                    }
                    else if(curr == '>' || curr == '<') {
                        if(fgetc(temp) == ' ') {
                            line_start = 0;
                        }
                        else{
                            //fprintf(stderr,"E 2");
                            return ERR;
                        }
                    }
                    else{
                        //fprintf(stderr,"E 3");
                        if(curr == EOF) {
                            return EOF;
                        }
                        return ERR;
                    }
                }
                index = 0;
            }
        }
        else if(curr == '\n'){
            line_start = 1;     
            //fprintf(stderr,"Line break");       
        }
        else{
            line_start = 0;
        }
        //fprintf(stderr,"%c ",curr);
        
    }
    return EOF;
}

/**
 * @brief  Get the next character from the data portion of the hunk.
 * @details  This function gets the next character from the data
 * portion of a hunk.  The data portion of a hunk consists of one
 * or both of a deletions section and an additions section,
 * depending on the hunk type (delete, append, or change).
 * Within each section is a series of lines that begin either with
 * the character sequence "< " (for deletions), or "> " (for additions).
 * For a change hunk, which has both a deletions section and an
 * additions section, the two sections are separated by a single
 * line containing the three-character sequence "---".
 * This function returns only characters that are actually part of
 * the lines to be deleted or added; characters from the special
 * sequences "< ", "> ", and "---\n" are not returned.
 * @param hdr  Data structure containing the header of the current
 * hunk.
 *
 * @param in  The stream from which hunks are being read.
 * @return  A character that is the next character in the current
 * line of the deletions section or additions section, unless the
 * end of the section has been reached, in which case the special
 * value EOS is returned.  If the hunk is ill-formed; for example,
 * if it contains a line that is not terminated by a newline character,
 * or if end-of-file is reached in the middle of the hunk, or a hunk
 * of change type is missing an additions section, then the special
 * value ERR (error) is returned.  The value ERR will also be returned
 * if this function is called after the current hunk has been completely
 * read, unless an intervening call to hunk_next() has been made to
 * advance to the next hunk in the input.  Once ERR has been returned,
 * then further calls to this function will continue to return ERR,
 * until a successful call to call to hunk_next() has successfully
 * advanced to the next hunk.
 */

static int linecount = 0;
static int linetotal = 0;

int hunk_getc(HUNK *hp, FILE *in) {
    HUNK hunk = *hp;
    int type = hunk.type;
    if(hunk.type == 1) {
            linetotal = hunk.new_end - hunk.new_start + 1;
        }
        else if(hunk.type == 2) {
            linetotal = hunk.old_end - hunk.old_start + 1;
        }
        else if(hunk.type == 3) {
            linetotal = hunk.old_end - hunk.old_start + 1 + hunk.new_end - hunk.new_start + 2;
        }
    //fprintf(stderr,"Linecount/Linetotal %d/%d", linecount,linetotal);
    if(linecount == linetotal) {
        linecount = 0;
        return EOS;
    }
    char thischar = fgetc(in);
    //fprintf(stderr,"Attempt X %c", thischar);
    //ADD
    if(type == 1) {
        if(line_start == 1) {
            if(thischar == '>') {
                if(fgetc(in) == ' ') {
                    //fprintf(stderr,"Start of add");
                    line_start = 0;
                    return hunk_getc(hp,in);
                }
                else{
                    return ERR;
                }
            }
            else{
                return ERR;
            }
        }
        else{
            if(thischar == '\n') {
                line_start = 1;
                linecount +=1;
            }
        }
    }

    //DELETE
    else if(type == 2) {
        if(line_start == 1) {
            if(thischar == '<') {
                if(fgetc(in) == ' ') {
                    //fprintf(stderr,"Start of del");
                    line_start = 0;
                    return hunk_getc(hp,in);
                }
                else{
                    return ERR;
                }
            }
            else{
                return ERR;
            }
        }
        else{
            if(thischar == '\n') {
                line_start = 1;
                linecount +=1;
            }
        }
    }

    //CHANGE
    else if(type == 3) {
        int halfway = hunk.old_end - hunk.old_start + 1;
        if(line_start == 1) {
            if(linecount < halfway) {
                //fprintf(stderr,"LINE %d %d", linecount, linetotal);
                if(thischar == '<') {
                    if(fgetc(in) == ' ') {
                        //fprintf(stderr,"Start of del");
                        line_start = 0;
                        return hunk_getc(hp,in);
                    }
                    else{
                        return ERR;
                    }
                }
                else{
                    return ERR;
                }
            }
            else if(linecount == halfway) {
                //fprintf(stderr,"HALWAY %d", halfway);
                if(thischar == '-') {
                    if(fgetc(in) == '-') {
                        if(fgetc(in) == '-') {
                            if(fgetc(in) == '\n') {
                                fprintf(stderr,"-----\n");
                                linecount+=1;
                                return hunk_getc(hp, in);
                            }
                        }
                    }
                }
            }
            else if(linecount > halfway) {
                //fprintf(stderr,"LINE %d %d", linecount, linetotal);
                if(thischar == '>') {
                    if(fgetc(in) == ' ') {
                        //fprintf(stderr,"Start of del");
                        line_start = 0;
                        return hunk_getc(hp,in);
                    }
                    else{
                        return ERR;
                    }
                }
                else{
                    return ERR;
                }
            }
        }
        else{
            if(thischar == '\n') {
                line_start = 1;
                linecount +=1;
            }
        }
    }

    //fprintf(stderr,"AT END %c",thischar);
    return thischar;
}

/**
 * @brief  Print a hunk to an output stream.
 * @details  This function prints a representation of a hunk to a
 * specified output stream.  The printed representation will always
 * have an initial line that specifies the type of the hunk and
 * the line numbers in the "old" and "new" versions of the file,
 * in the same format as it would appear in a traditional diff file.
 * The printed representation may also include portions of the
 * lines to be deleted and/or inserted by this hunk, to the extent
 * that they are available.  This information is defined to be
 * available if the hunk is the current hunk, which has been completely
 * read, and a call to hunk_next() has not yet been made to advance
 * to the next hunk.  In this case, the lines to be printed will
 * be those that have been stored in the hunk_deletions_buffer
 * and hunk_additions_buffer array.  If there is no current hunk,
 * or the current hunk has not yet been completely read, then no
 * deletions or additions information will be printed.
 * If the lines stored in the hunk_deletions_buffer or
 * hunk_additions_buffer array were truncated due to there having
 * been more data than would fit in the buffer, then this function
 * will print an elipsis "..." followed by a single newline character
 * after any such truncated lines, as an indication that truncation
 * has occurred.
 *
 * @param hp  Data structure giving the header information about the
 * hunk to be printed.
 * @param out  Output stream to which the hunk should be printed.
 */

void hunk_show(HUNK *hp, FILE *out) {
    // TO BE IMPLEMENTED
    abort();
}

/**
 * @brief  Patch a file as specified by a diff.
 * @details  This function reads a diff file from an input stream
 * and uses the information in it to transform a source file, read on
 * another input stream into a target file, which is written to an
 * output stream.  The transformation is performed "on-the-fly"
 * as the input is read, without storing either it or the diff file
 * in memory, and errors are reported as soon as they are detected.
 * This mode of operation implies that in general when an error is
 * detected, some amount of output might already have been produced.
 * In case of a fatal error, processing may terminate prematurely,
 * having produced only a truncated version of the result.
 * In case the diff file is empty, then the output should be an
 * unchanged copy of the input.
 *
 * This function checks for the following kinds of errors: ill-formed
 * diff file, failure of lines being deleted from the input to match
 * the corresponding deletion lines in the diff file, failure of the
 * line numbers specified in each "hunk" of the diff to match the line
 * numbers in the old and new versions of the file, and input/output
 * errors while reading the input or writing the output.  When any
 * error is detected, a report of the error is printed to stderr.
 * The error message will consist of a single line of text that describes
 * what went wrong, possibly followed by a representation of the current
 * hunk from the diff file, if the error pertains to that hunk or its
 * application to the input file.  If the "quiet mode" program option
 * has been specified, then the printing of error messages will be
 * suppressed.  This function returns immediately after issuing an
 * error report.
 *
 * The meaning of the old and new line numbers in a diff file is slightly
 * confusing.  The starting line number in the "old" file is the number
 * of the first affected line in case of a deletion or change hunk,
 * but it is the number of the line *preceding* the addition in case of
 * an addition hunk.  The starting line number in the "new" file is
 * the number of the first affected line in case of an addition or change
 * hunk, but it is the number of the line *preceding* the deletion in
 * case of a deletion hunk.
 *
 * @param in  Input stream from which the file to be patched is read.
 * @param out Output stream to which the patched file is to be written.
 * @param diff  Input stream from which the diff file is to be read.
 * @return 0 in case processing completes without any errors, and -1
 * if there were errors.  If no error is reported, then it is guaranteed
 * that the output is complete and correct.  If an error is reported,
 * then the output may be incomplete or incorrect.
 */

int patch(FILE *in, FILE *out, FILE *diff) {
    char cchar;
    //printf("GUm");
    //while(cchar != EOF) {
    //    printf("J");
    //    cchar = fgetc(temp);
    //    fprintf(out,"%c",cchar);
    //}
    //printf("after");
    HUNK hunk;
    HUNK* curr = &hunk;
    int nextval;
    int getval;
    while((nextval = hunk_next(curr,diff)) != EOF) {
        if(nextval == ERR) {
            fprintf(stderr,"Error %d", fgetc(in));
            break;
        }
        fprintf(stderr,"HUNK STATS: %d ",nextval);
        fprintf(stderr," %d ",(*curr).type);
        fprintf(stderr," %d ",(*curr).serial);
        fprintf(stderr," %d ",(*curr).old_start);
        fprintf(stderr," %d ",(*curr).old_end);
        fprintf(stderr," %d ",(*curr).new_start);
        fprintf(stderr," %d \n",(*curr).new_end);
        // linecount = 0;
        // if(hunk.type == 1) {
        //     linetotal = hunk.new_end - hunk.new_start + 1;
        // }
        // else if(hunk.type == 2) {
        //     linetotal = hunk.old_end - hunk.old_start + 1;
        // }
        // else if(hunk.type == 3) {
        //     linetotal = hunk.old_end - hunk.old_start + 1 + hunk.new_end - hunk.new_start + 2;
        // }
        while((getval = hunk_getc(curr,diff)) >= 0) {
            //fprintf(stderr,"Attempt");
            fprintf(stderr,"%c",getval);
        }
        fprintf(stderr,"next hunk\n");
    }
    fprintf(stderr,"DONE");
    return 0;
}
