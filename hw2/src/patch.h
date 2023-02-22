typedef long LINENUM;

void plan_b(char *filename);
void open_patch_file(char *filename);
void copy_file(char *from,char *to);
void move_file(char *from,char *to);
void copy_till(register LINENUM lastline);
void get_some_switches();
void vsay(char *pat, va_list args);
void say(char *pat,...);
void fatal(char *pat,...);
void ask(char *pat,...);