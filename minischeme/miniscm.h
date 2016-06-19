typedef struct cell *pointer;

pointer nil_pointer();
pointer car(pointer);
pointer cdr(pointer);
pointer cadr(pointer);
pointer caddr(pointer);
pointer cons(pointer,pointer);
bool is_symbol(pointer);
char* sym_name(pointer);
pointer Error_1(char *,pointer);
pointer Error_0(char *);
pointer mk_symbol(char*);
pointer mk_integer(int);
pointer mk_extra();
int list_length(pointer);
int int_value(pointer);
char *string_value(pointer);
pointer list_ref(pointer,int);

pointer list_to_undef(pointer a);  // extra functions
pointer list_to_int(pointer a);
pointer list_to_list(pointer a);
