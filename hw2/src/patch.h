typedef long LINENUM;
typedef char bool;

bool plan_a(char *filename);
void plan_b(char *filename);
void open_patch_file(char *filename);
void copy_file(char *from,char *to);
void move_file(char *from,char *to);
void copy_till(register LINENUM lastline);
void get_some_switches();
void free_almost_everything();
void vsay(char *pat, va_list args);
void say(char *pat,...);
void fatal(char *pat,...);
void ask(char *pat,...);
void reinitialize_almost_everything();
void init_output(char *name);
void do_ed_script();
void init_reject(char *name);
void scan_input(char *filename);
void pch_swap();
void abort_hunk();
void apply_hunk(LINENUM where);
void spew_output();
void ignore_signals();
void re_patch();
void re_input();
void skip_to(long file_pos);
void next_intuit_at(long file_pos);
int intuit_diff_type();
void dump_line(LINENUM line);
char* savestr(register char *s);

void get_some_switches2();