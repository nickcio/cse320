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

static int permanent_error = 0;
static int line_start = 1;
static int serial = 0;
static int linebreaks = 0;
static int prevwasn = 0;
static int very_previous_char = 0;
static char* addbuff = hunk_additions_buffer+2;
static char* addbuffcount = hunk_additions_buffer;
static char* delbuff = hunk_deletions_buffer+2;
static char* delbuffcount = hunk_deletions_buffer;

int hunk_next(HUNK *hp, FILE *in) {
    if(permanent_error) {
        return ERR;
    }
    FILE *temp = in;
    char curr;
    int index = 0;

    int in_num = 0;
    int type = 0;
    int firstin1 = -1;
    int lastin1 = -1;
    int firstin2 = -1;
    int lastin2 = -1;
    //fprintf(stderr,"LASTCHAR INITIALIZED \n");

    int clearcount = 0;
    char* clearpoint = hunk_additions_buffer;
    while(clearcount < HUNK_MAX) {
        *clearpoint = 0x0;
        clearpoint++;
        clearcount++;
    }

    clearcount = 0;
    clearpoint = hunk_deletions_buffer;
    while(clearcount < HUNK_MAX) {
        *clearpoint = 0x0;
        clearpoint++;
        clearcount++;
    }

    addbuff = hunk_additions_buffer+2;
    addbuffcount = hunk_additions_buffer;
    delbuff = hunk_deletions_buffer+2;
    delbuffcount = hunk_deletions_buffer;

    while(curr != EOF){
        //fprintf(stderr,"MILESTONE");
        //fprintf(stderr,"%c",curr);
        curr = fgetc(temp);
        very_previous_char = curr != EOF ? curr : very_previous_char;
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
                        (*hp).serial = hp->serial+1;
                        (*hp).old_start = firstin1;
                        (*hp).old_end = lastin1 >= 0 ? lastin1 : firstin1;
                        (*hp).new_start = firstin2;
                        (*hp).new_end = lastin2 >= 0 ? lastin2 : firstin2;
                        (*hp).type = type == 1 ? HUNK_APPEND_TYPE : type == 2 ? HUNK_DELETE_TYPE : type == 3 ? HUNK_CHANGE_TYPE: HUNK_NO_TYPE;
                        return permanent_error ? ERR : 0;
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
                        //fprintf(stderr,"grump");
                        if(fgetc(temp) == ' ') {
                            line_start = 0;
                        }
                        else{
                            //fprintf(stderr,"BROKE");
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
            linebreaks++;
            //fprintf(stderr,"Line break");       
        }
        else{
            line_start = 0;
        }
        //fprintf(stderr,"%c ",curr);
        
        //lastbefore = lastchar;
        //lastchar = curr;
        //fprintf(stderr,"SETTING LAST CHAR! %c %d\n", lastchar, lastchar);
    }
    very_previous_char = curr != EOF ? curr : very_previous_char;
    //fprintf(stderr,"EDONE %c %c END\n", lastchar, lastbefore);
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
static int changehalf = 0;

int hunk_getc(HUNK *hp, FILE *in) {
    if(permanent_error) {
        return ERR;
    }
    HUNK hunk = *hp;
    int type = hunk.type;
    int halfway = hunk.old_end - hunk.old_start + 1;
    //fprintf(stderr,"Linecount/Linetotal %d/%d", linecount,linetotal);

    char thischar = fgetc(in);
    int num = thischar-48;
    //fprintf(stderr,"Attempt X %c", thischar);
    //ADD
    if(line_start && (num >= 0 && num <= 9)) {
        //fprintf(stderr,"HERE1");
        if(type == 3 && changehalf == 0) {
            //fprintf(stderr,"HERE2");
            permanent_error=1;
            return ERR;
        }
        ungetc(thischar,in);
        changehalf = 0;
        return EOS;
    }

    if(line_start && thischar == '-') {
        //fprintf(stderr,"HERE3");
        if(type == 3 && changehalf == 1) {
            //fprintf(stderr,"HERE4");
            permanent_error=1;
            return ERR;
        }
        if(fgetc(in) == '-') {
            //fprintf(stderr,"HERE5");
            if(fgetc(in) == '-') {
                //fprintf(stderr,"HERE6");
                if(fgetc(in) == '\n') {
                    //fprintf(stderr,"HERE7 CHANGEHALF=%d ",changehalf);
                    changehalf = 1;
                    return EOS;
                }
                else{
                    permanent_error=1;
                    return ERR;
                }
            }
            else{
                permanent_error=1;
                return ERR;
            }
        }
        else{
            permanent_error=1;
            return ERR;
        }
    }
    
    if(line_start && thischar == EOF) {
        if(type == 3 && changehalf == 0) {
            permanent_error=1;
            return ERR;
        }
        changehalf = 0;
        ungetc(thischar,in);
        return EOS;
    }

    if(type == 1) {
        if(line_start == 1) {
            if(thischar == '>') {
                if(fgetc(in) == ' ') {
                    //fprintf(stderr,"Start of add");
                    line_start = 0;
                    return hunk_getc(hp,in);
                }
                else{
                    permanent_error=1;
                    return ERR;
                }
            }
            else{
                permanent_error=1;
                return ERR;
            }
        }
        else{
            if(*(hunk_additions_buffer+HUNK_MAX-3) == 0x0) {
                *addbuff = thischar;
                //fprintf(stderr,"BUFFR COUNT: %u / %u", (*addbuffcount),256);
                if(((unsigned char) *addbuffcount) < 255){
                    (*addbuffcount)++;
                }
                else{
                    //fprintf(stderr,"OVEFLOW!!");
                    (*(addbuffcount+1))++;
                    (*addbuffcount) = 0;
                }
                addbuff++;
            }
            if(thischar == '\n') {
                line_start = 1;
                if(*(hunk_additions_buffer+HUNK_MAX-3) == 0x0) {
                    addbuffcount = addbuff;
                    addbuff+=2;
                }
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
                    permanent_error=1;
                    return ERR;
                }
            }
            else{
                permanent_error=1;
                return ERR;
            }
        }
        else{
            if(*(hunk_deletions_buffer+HUNK_MAX-3) == 0x0) {
                *delbuff = thischar;
                if((unsigned char) *delbuffcount < 255){
                    (*delbuffcount)++;
                }
                else{
                    (*(delbuffcount+1))++;
                    (*delbuffcount) = 0;
                }
                delbuff++;
            }
            if(thischar == '\n') {
                line_start = 1;
                if(*(hunk_deletions_buffer+HUNK_MAX-3) == 0x0) {
                    delbuffcount = delbuff;
                    delbuff+=2;
                }
            }
        }
    }

    //CHANGE
    else if(type == 3) {
        if(line_start == 1) {
            if(!changehalf) {
                //fprintf(stderr,"LINE %d %d", linecount, linetotal);
                if(thischar == '<') {
                    if(fgetc(in) == ' ') {
                        //fprintf(stderr,"Start of del");
                        line_start = 0;
                        return hunk_getc(hp,in);
                    }
                    else{
                        permanent_error=1;
                        return ERR;
                    }
                }
            }
            else if(changehalf) {
                //fprintf(stderr,"LINE %d %d", linecount, linetotal);
                if(thischar == '>') {
                    if(fgetc(in) == ' ') {
                        //fprintf(stderr,"Start of del");
                        line_start = 0;
                        return hunk_getc(hp,in);
                    }
                    else{
                        //fprintf(stderr,"HEREX");
                        permanent_error=1;
                        return ERR;
                    }
                }
                else{
                    //fprintf(stderr,"HEREY %c %d ",thischar,num);
                    permanent_error=1;
                    return ERR;
                }
            }
        }
        else{
            if(!changehalf) {
                if(*(hunk_deletions_buffer+HUNK_MAX-3) == 0x0 && delbuff < hunk_deletions_buffer+HUNK_MAX-2) {
                    *delbuff = thischar;
                    if((unsigned char) *delbuffcount < 255){
                        (*delbuffcount)++;
                    }
                    else{
                        (*(delbuffcount+1))++;
                        (*delbuffcount) = 0;
                    }
                    delbuff++;
                }
                if(thischar == '\n') {
                    line_start = 1;
                    if(*(hunk_deletions_buffer+HUNK_MAX-3) == 0x0 && delbuff < hunk_deletions_buffer+HUNK_MAX-2) {
                        delbuffcount = delbuff;
                        delbuff+=2;
                    }
                }
            }
            else if(changehalf) {
                if(*(hunk_additions_buffer+HUNK_MAX-3) == 0x0 && addbuff < hunk_additions_buffer+HUNK_MAX-2) {
                    *addbuff = thischar;
                    if((unsigned char) *addbuffcount < 255){
                        (*addbuffcount)++;
                    }
                    else{
                        (*(addbuffcount+1))++;
                        (*addbuffcount) = 0;
                    }
                    addbuff++;
                }
                if(thischar == '\n') {
                    line_start = 1;
                    if(*(hunk_additions_buffer+HUNK_MAX-3) == 0x0 && addbuff < hunk_additions_buffer+HUNK_MAX-2) {
                        addbuffcount = addbuff;
                        addbuff+=2;
                    }
                }
            }
            else if(thischar == '\n') {
                line_start = 1;
            }
        }
    }
    very_previous_char = thischar;
    //fprintf(stderr,"AT END %c",thischar);
    return permanent_error ? ERR : thischar;
    //return thischar;
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
    int type = hp->type;
    char* delprint = hunk_deletions_buffer;
    char* addprint = hunk_additions_buffer;
    if(hp->old_start != hp->old_end) {
        fprintf(out,"%d,",hp->old_start);
    }
    fprintf(out,"%d",hp->old_end);
    if(type == 1) {
        fprintf(out,"a");
    }
    else if(type == 2) {
        fprintf(out,"d");
    }
    else if(type == 3) {
        fprintf(out,"c");
    }
    if(hp->new_start != hp->new_end) {
        fprintf(out,"%d,",hp->new_start);
    }
    fprintf(out,"%d\n", hp->new_end);
    while((*delprint) != 0 || (*(delprint+1)) != 0) {
        int fullcount = ((unsigned char) (*delprint) ) + ((unsigned char) (*(delprint+1)) )*256;
        delprint+=2;
        fprintf(out,"< ");
        while(fullcount > 0){
            fprintf(out,"%c",(*delprint));
            delprint++;
            fullcount--;
        }
    }
    if(delprint == hunk_deletions_buffer+HUNK_MAX-2) {
        fprintf(out,"...\n");
    }
    if(type == 3) {
        fprintf(out,"---\n");
    }
    while((*addprint) != 0 || (*(addprint+1)) != 0) {
        int fullcount = ((unsigned char) (*addprint) ) + ((unsigned char) (*(addprint+1)) )*256;
        addprint+=2;
        fprintf(out,"> ");
        while(fullcount > 0){
            fprintf(out,"%c",(*addprint));
            addprint++;
            fullcount--;
        }
    }
    if(addprint == hunk_additions_buffer+HUNK_MAX-2) {
        fprintf(out,"...\n");
    }
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
    hunk.old_end= 0;
    hunk.new_end= 0;
    hunk.old_start= 0;
    hunk.new_start= 0;
    hunk.type = 0;
    hunk.serial = -1;
    HUNK* curr = &hunk;
    int nextval;
    int getval;
    int hunklen = 0;
    int lines_count;
    int lefthighest = -1;
    int righthighest = -1;
    linebreaks = 0;
    int in_i = 0;
    while((nextval = hunk_next(curr,diff)) != EOF) {
        if(nextval == ERR) {
            permanent_error = 1;
            if((global_options & QUIET_OPTION )!= QUIET_OPTION) {
                fprintf(stderr,"Syntax Error!\n");
                hunk_show(curr,stderr);
            }
            return -1;
        }

        //fprintf(stderr,"HUNK STATS: %d ",nextval);
        //fprintf(stderr," %d ",(*curr).type);
        //fprintf(stderr," %d ",(*curr).serial);
        //fprintf(stderr," %d ",(*curr).old_start);
        //fprintf(stderr," %d ",(*curr).old_end);
        //fprintf(stderr," %d ",(*curr).new_start);
        //fprintf(stderr," %d \n",(*curr).new_end);

        if(hunk.old_end <= lefthighest || hunk.new_end <= righthighest) {
            return -1;
        }
        lefthighest = hunk.old_end;
        righthighest = hunk.new_end;
        if(hunk.type == 1) {
            hunklen = hunk.new_end - hunk.new_start + 1;
        }
        else if(hunk.type == 2) {
            hunklen = hunk.old_end - hunk.old_start + 1;
        }
        else if(hunk.type == 3) {
            hunklen = hunk.old_end - hunk.old_start + 1 + hunk.new_end - hunk.new_start + 1;
        }
        lines_count = 0;
        while((getval = hunk_getc(curr,diff)) >= 0) {
            if(getval == '\n') {
                lines_count++;
            }
        }
        if(getval == ERR) {
            if((global_options & QUIET_OPTION) != QUIET_OPTION) {
                fprintf(stderr,"Syntax Error!\n");
                hunk_show(curr,stderr);
            }
            return -1;
        }
        //SECOND HALF OF CHANGE HUNK!
        if(hunk.type == 3) {
            //fprintf(stderr,"SECOND HALF\n");
            while((getval = hunk_getc(curr,diff)) >= 0) {
                //fprintf(stderr,"%d/%c: ",getval,getval);
                if(getval == '\n') {
                    lines_count++;
                }
            }
            //fprintf(stderr,"last: %c/%d",getval,getval);
            if(getval == ERR) {
                if((global_options & QUIET_OPTION) != QUIET_OPTION) {
                    fprintf(stderr,"Syntax Error!\n");
                    hunk_show(curr,stderr);
                }
                return -1;
            }
        }
        if(very_previous_char != '\n') {
            linebreaks++;
        }
        //fprintf(stderr,"LINES: %d =? %d\n",hunklen,lines_count+linebreaks);
        if(hunklen !=0 && hunklen != lines_count+linebreaks) {
            if((global_options & QUIET_OPTION) != QUIET_OPTION) {
                fprintf(stderr,"Incorrect Line Count!\n");
                hunk_show(curr,stderr);
            }
            return -1;
        }
        //hunk_show(curr,stderr);

        //PATCHING
        if((global_options & NO_PATCH_OPTION) != NO_PATCH_OPTION) {
            char* addpatch = hunk_additions_buffer;
            if(curr->type == 1) {
                while(in_i < curr->old_start) {
                    char thisval;
                    while((thisval = fgetc(in))) {
                        if(thisval != EOF)
                            fprintf(out,"%c",thisval);
                        if(thisval == '\n' || thisval == EOF) {
                            in_i++;
                            break;
                        }
                    }
                }
                while((*addpatch) != 0 || (*(addpatch+1)) != 0) {
                    int fullcount = ((unsigned char) (*addpatch) ) + ((unsigned char) (*(addpatch+1)) )*256;
                    addpatch+=2;
                    while(fullcount > 0){
                        fprintf(out,"%c",(*addpatch));
                        addpatch++;
                        fullcount--;
                    }
                }
            }
            else if(curr->type == 2) {
                while(in_i < curr->old_start-1) {
                    char thisval;
                    while((thisval = fgetc(in))) {
                        if(thisval != EOF)
                            fprintf(out,"%c",thisval);
                        if(thisval == '\n' || thisval == EOF) {
                            in_i++;
                            break;
                        }
                    }
                }
                while(in_i < curr->old_end) {
                    char thisval;
                    while((thisval = fgetc(in))) {
                        //fprintf(out,"%c",thisval);
                        if(thisval == '\n' || thisval == EOF) {
                            in_i++;
                            break;
                        }
                    }
                }
            }
            if(curr->type == 3) {
                while(in_i < curr->old_start-1) {
                    char thisval;
                    while((thisval = fgetc(in))) {
                        if(thisval != EOF)
                            fprintf(out,"%c",thisval);
                        if(thisval == '\n' || thisval == EOF) {
                            in_i++;
                            break;
                        }
                    }
                }
                while(in_i <= curr->old_end-1) {
                    char thisval;
                    while((thisval = fgetc(in))) {
                        //fprintf(out,"%c",thisval);
                        if(thisval == '\n' || thisval == EOF) {
                            in_i++;
                            break;
                        }
                    }
                }
                while((*addpatch) != 0 || (*(addpatch+1)) != 0) {
                    int fullcount = ((unsigned char) (*addpatch) ) + ((unsigned char) (*(addpatch+1)) )*256;
                    addpatch+=2;
                    while(fullcount > 0){
                        fprintf(out,"%c",(*addpatch));
                        addpatch++;
                        fullcount--;
                    }
                }
            }
            if(addpatch == hunk_additions_buffer+HUNK_MAX-2) {
            fprintf(out,"...\n");
            }
        }

        //fprintf(stderr,"\nnext hunk\n");
    }
    if(very_previous_char != '\n') {
        if((global_options & QUIET_OPTION) != QUIET_OPTION) {
            fprintf(stderr,"Syntax Error!\n");
            hunk_show(curr,stderr);
        }
        return -1;
    }
    if(linebreaks > 0) {
        if((global_options & QUIET_OPTION) != QUIET_OPTION) {
            fprintf(stderr,"Incorrect Line Count!\n");
            hunk_show(curr,stderr);
        }
        return -1;
    }
        char thisval;
        while((global_options & NO_PATCH_OPTION) != NO_PATCH_OPTION && (thisval = fgetc(in))) {
            if(thisval != EOF)
                fprintf(out,"%c",thisval);
            else {
                break;
            }
        }

    //fprintf(stderr,"Line breaks: %u, %u DONE", linebreaks, very_previous_char);
    return 0;
}
