/*
 *      ---------- Mini-Scheme Interpreter Version 0.9 ----------
 *
 *                coded by Atsushi Moriwaki (11/5/1989)
 *
 *               THIS SOFTWARE IS IN THE PUBLIC DOMAIN
 *               ------------------------------------
 * This software is completely free to copy, modify and/or re-distribute.
 * But I would appreciate it if you left my name on the code as the author.
 *
 *  This version has been modified by R.C. Secrist.
 *
 *  This is a revised and modified version by Akira KIDA.
 *	(15 May 1994)
 *
 *  Ported to ANSI C by W.Boeke.
 *	Foreign C interface and get_line (with history) added, also stole some
 *  code from TinyScheme by D.Souflis.
 *  (march 2004)
 */ 

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

#ifndef __cplusplus
  typedef char bool;
#endif
#include "miniscm.h"
#include "get_line.h"

// max: 200,000 cells = 2,400,000 bytes
#define CELL_SEGSIZE    10000	/* # of cells in one segment */
#define CELL_NSEGMENT   20   	/* # of segments for cells */
#define FIRST_CELLSEGS 5

#define banner "Hello, this is Mini-Scheme"
#define InitFile "init.scm"

/* cell structure */
typedef struct cell {
	unsigned short _flag;
	union {
		struct {
			char *_svalue;
			short _keynum;
		} _string;
		int _ivalue;
		struct {
			struct cell *_car;
			struct cell *_cdr;
		} _cons;
		FILE *_fp;
	} _object;
} cell;

#define T_STRING           1	/* 0000000000000001 */
#define T_NUMBER           2	/* 0000000000000010 */
#define T_SYMBOL           4	/* 0000000000000100 */
#define T_SYNTAX           8	/* 0000000000001000 */
#define T_PROC          0x10	/* 0000000000010000 */
#define T_PAIR          0x20	/* 0000000000100000 */
#define T_CLOSURE       0x40 	/* 0000000001000000 */
#define T_CONTINUATION  0x80	/* 0000000010000000 */
#define T_MACRO        0x100	/* 0000000100000000 */
#define T_PROMISE      0x200	/* 0000001000000000 */
#define T_FILE         0x400	/* 0000010000000000 */
#define T_EXTRA        0x800	/* 0000100000000000 */
#define T_ATOM        0x4000	/* 0100000000000000 */	/* only for gc */
#define CLRATOM      ~0x4000	/* 1011111111111111 */	/* only for gc */
#define MARK          0x8000	/* 1000000000000000 */
#define UNMARK       ~0x8000	/* 0111111111111111 */

/* operator code */
enum oper_code {
	OP_LOAD,  // should be 0 because this value is returned by setjmp() at startup.
	OP_T0LVL,
	OP_T1LVL,
	OP_READ,
	OP_VALUEPRINT,
	OP_EVAL,
	OP_E0ARGS,
	OP_E1ARGS,
	OP_APPLY,
	OP_DOMACRO,

	OP_LAMBDA,
	OP_QUOTE,
	OP_DEF0,
	OP_DEF1,
	OP_BEGIN,
	OP_IF0,
	OP_IF1,
	OP_SET0,
	OP_SET1,
	OP_LET0,
	OP_LET1,
	OP_LET2,
	OP_LET0AST,
	OP_LET1AST,
	OP_LET2AST,
	OP_LET0REC,
	OP_LET1REC,
	OP_LET2REC,
	OP_COND0,
	OP_COND1,
	OP_DELAY,
	OP_AND0,
	OP_AND1,
	OP_OR0,
	OP_OR1,
	OP_C0STREAM,
	OP_C1STREAM,
	OP_0MACRO,
	OP_1MACRO,
	OP_CASE0,
	OP_CASE1,
	OP_CASE2,

	OP_PEVAL,
	OP_PAPPLY,
	OP_CONTINUATION,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_REM,
	OP_CAR,
	OP_CDR,
	OP_CONS,
	OP_SETCAR,
	OP_SETCDR,
	OP_NOT,
	OP_BOOL,
	OP_NULL,
	OP_ZEROP,
	OP_POSP,
	OP_NEGP,
	OP_NEQ,
	OP_LESS,
	OP_GRE,
	OP_LEQ,
	OP_GEQ,
	OP_SYMBOL,
	OP_NUMBER,
	OP_STRING,	
	OP_PROC,
	OP_PAIR,
	OP_EQ,
	OP_EQV,
	OP_FORCE,
	OP_WRITE,
	OP_DISPLAY,
	OP_NEWLINE,
	OP_ERR0,
	OP_ERR1,
	OP_REVERSE,
	OP_APPEND,
	OP_PUT,
	OP_GET,
	OP_QUIT,
	OP_GC,
	OP_GCVERB,
	OP_NEWSEGMENT,

	OP_RDSEXPR,
	OP_RDLIST,
	OP_RDDOT,
	OP_RDQUOTE,
	OP_RDQQUOTE,
	OP_RDUNQUOTE,
	OP_RDUQTSP,

	OP_P0LIST,
	OP_P1LIST,

	OP_LIST_LENGTH,
	OP_LIST_REF,
	OP_ASSQ,
//	OP_PRINT_WIDTH,
//	OP_P0_WIDTH,
//	OP_P1_WIDTH,
	OP_GET_CLOSURE,
	OP_CLOSUREP,
	OP_MACROP,

    OP_LIST_UNDEF,  // extra functions
    OP_LIST_INT,
    OP_LIST_LIST,

    OP_MAXDEFINED
};

/* macros for cell operations */
#define type(p)         ((p)->_flag)

#define isstring(p)     (type(p)&T_STRING)
#define strvalue(p)     ((p)->_object._string._svalue)
#define keynum(p)       ((p)->_object._string._keynum)

#define isnumber(p)     (type(p)&T_NUMBER)
#define ivalue(p)       ((p)->_object._ivalue)

#define isfile(p)       (type(p)&T_FILE)
#define fpvalue(p)      ((p)->_object._fp)

#define ispair(p)       (type(p)&T_PAIR)
#define car(p)          ((p)->_object._cons._car)
#define cdr(p)          ((p)->_object._cons._cdr)

#define issymbol(p)     (type(p)&T_SYMBOL)
#define symname(p)      strvalue(car(p))
#define hasprop(p)      (type(p)&T_SYMBOL)
#define symprop(p)      cdr(p)

#define issyntax(p)     (type(p)&T_SYNTAX)
#define isproc(p)       (type(p)&T_PROC)
#define syntaxname(p)   strvalue(car(p))
#define syntaxnum(p)    keynum(car(p))
#define procnum(p)      ivalue(p)

#define isclosure(p)    (type(p)&T_CLOSURE)
#define ismacro(p)      (type(p)&T_MACRO)
#define closure_code(p) car(p)
#define closure_env(p)  cdr(p)

#define iscontinuation(p) (type(p)&T_CONTINUATION)
#define cont_dump(p)    cdr(p)

#define ispromise(p)    (type(p)&T_PROMISE)
#define setpromise(p)   type(p) |= T_PROMISE

#define isatom(p)       (type(p)&T_ATOM)
#define setatom(p)      type(p) |= T_ATOM
#define clratom(p)      type(p) &= CLRATOM

#define ismark(p)       (type(p)&MARK)
#define setmark(p)      type(p) |= MARK
#define clrmark(p)      type(p) &= UNMARK

#define isextra(p)      (type(p) & T_EXTRA)

#define caar(p)         car(car(p))
#define cadr(p)         car(cdr(p))
#define cdar(p)         cdr(car(p))
#define cddr(p)         cdr(cdr(p))
#define cadar(p)        car(cdr(car(p)))
#define caddr(p)        car(cdr(cdr(p)))
#define cadaar(p)       car(cdr(car(car(p))))
#define cadddr(p)       car(cdr(cdr(cdr(p))))
#define cddddr(p)       cdr(cdr(cdr(cdr(p))))

// Global variables
int tok,
    oper,
    last_cell_seg = -1,
    fcells = 0;		/* # of free cells */

bool print_flag,
     gc_verbose = 0;

/* arrays for segments */
pointer cell_seg[CELL_NSEGMENT];

/* We use 4 registers. */
pointer args,			// register for arguments of function
        envir,			// stack register for current environment
        code,			// register for current code
        dump,			// stack register for next evaluation

        file_args,		// input files
        value;

cell _NIL,
     _T,
     _F,
     _NOP;

pointer F = &_F,		/* special cell representing #f */
        T = &_T,		/* special cell representing #t */
        NIL = &_NIL,	/* special cell representing empty cell */
        NOP = &_NOP,
        oblist = &_NIL,	/* pointer to symbol table */
        global_env;		/* pointer to global environment */

/* global pointers to special symbols */
pointer LAMBDA,			/* pointer to syntax lambda */
        QUOTE,			/* pointer to syntax quote */
        QQUOTE,			/* pointer to symbol quasiquote */
        UNQUOTE,		/* pointer to symbol unquote */
        UNQUOTESP,		/* pointer to symbol unquote-splicing */
        free_cell = &_NIL;/* pointer to top of free cells */

FILE   *infp;			/* input file */
jmp_buf error_jmp;

/* allocate new cell segment */
int alloc_cellseg(int n)
{
	pointer p;
	int i, k;

	for (k = 0; k < n; k++) {
		if (last_cell_seg >= CELL_NSEGMENT - 1) {
			if (gc_verbose)
				printf("Only %d cell segments added\n", k);
			return k;
		}
		p = (pointer) malloc(CELL_SEGSIZE * sizeof(cell));
		if (!p)
			return k;
		cell_seg[++last_cell_seg] = p;
		fcells += CELL_SEGSIZE;
		for (i = 0; i < CELL_SEGSIZE - 1; i++, p++) {
			type(p) = 0;
			car(p) = NIL;
			cdr(p) = p + 1;
		}
		type(p) = 0;
		car(p) = NIL;
		cdr(p) = free_cell;
		free_cell = cell_seg[last_cell_seg];
	}
	if (gc_verbose)
		printf("%d cell segments added\n", n);
	return n;
}

void FatalError(char *fmt)
{
	fputs("Fatal error: ",stdout);
	puts(fmt);
	exit(1);
}

/* ========== garbage collector ========== */

/*--
 *  We use algorithm E (Kunuth, The Art of Computer Programming Vol.1,
 *  sec.3.5) for marking.
 */

void mark(pointer a)
{
	pointer t, q, p;

E1:	t = 0;
	p = a;
E2:	setmark(p);
E3:	if (isatom(p))
		goto E6;
E4:	q = car(p);
	if (q && !ismark(q)) {
		setatom(p);
		car(p) = t;
		t = p;
		p = q;
		goto E2;
	}
E5:	q = cdr(p);
	if (q && !ismark(q)) {
		cdr(p) = t;
		t = p;
		p = q;
		goto E2;
	}
E6:	if (!t)
		return;
	q = t;
	if (isatom(q)) {
		clratom(q);
		t = car(q);
		car(q) = p;
		p = q;
		goto E5;
	} else {
		t = cdr(q);
		cdr(q) = p;
		p = q;
		goto E6;
	}
}

/* garbage collection. parameter a, b is marked. */
void gc(pointer a,pointer b)
{
	pointer p;
	int i, j;

	if (gc_verbose)
		fputs("gc...",stdout);

	/* mark system globals */
	mark(oblist);
	mark(global_env);

	/* mark current registers */
	mark(args);
	mark(envir);
	mark(code);
	mark(dump);
	mark(file_args);

	/* mark variables a, b */
	mark(a);
	mark(b);

	/* garbage collect */
	clrmark(NIL);
	fcells = 0;
	free_cell = NIL;
	for (i = 0; i <= last_cell_seg; i++) {
		for (j = 0, p = cell_seg[i]; j < CELL_SEGSIZE; j++, p++) {
			if (ismark(p))
				clrmark(p);
			else {
				type(p) = 0;
				cdr(p) = free_cell;
				car(p) = NIL;
				free_cell = p;
				++fcells;
			}
		}
	}

	if (gc_verbose)
		printf(" done %d cells are recovered.\n", fcells);
}

/* get new cell.  parameter a, b is marked by gc. */
pointer get_cell(pointer a, pointer b)
{
	pointer x;

	if (free_cell == NIL) {
		gc(a, b);
		if (fcells<CELL_SEGSIZE || free_cell == NIL)
			if (!alloc_cellseg(1))
				FatalError("run out of cells  --- unable to recover cells");
	}
	x = free_cell;
	free_cell = cdr(x);
	--fcells;
	return x;
}

/* get new cons cell */
pointer cons(pointer a, pointer b)
{
	pointer x = get_cell(a, b);

	type(x) = T_PAIR;
	car(x) = a;
	cdr(x) = b;
	return x;
}

typedef struct name_node {
  char *name;
  struct name_node *left,*right;
} name_node;

char *find_name(const char *name) {
  int comp;
  static name_node *name_top;
  name_node *np=name_top;
  if (!np) {
    name_top=np=(name_node *)calloc(1,sizeof(name_node));
    return (np->name=strdup(name));
  }
  for (;;) {
    comp=strcmp(np->name,name);
    if (comp<0) {
      if (!np->left) {
        np=np->left=(name_node *)calloc(1,sizeof(name_node));
        return (np->name=strdup(name));
      }
      np=np->left;
    }
    else if (comp>0) {
      if (!np->right) {
        np=np->right=(name_node *)calloc(1,sizeof(name_node));
        return (np->name=strdup(name));
      }
      np=np->right;
    }
    else
      return np->name;
  }
}

/* get new string */
pointer mk_string(char *str)
{
	pointer x = get_cell(NIL, NIL);

	strvalue(x) = find_name(str);
	type(x) = T_STRING | T_ATOM;
	keynum(x) = -1;
	return x;
}

/* get new symbol */
pointer mk_symbol(char   *name)
{
	pointer x;
    name=find_name(name);

	/* first check oblist */
	for (x = oblist; x != NIL; x = cdr(x))
		if (name==symname(car(x)))  // names are unique
//		if (!strcmp(name, symname(car(x))))
			break;

	if (x != NIL)
		return car(x);
	else {
		x = cons(mk_string(name), NIL);
		type(x) = T_SYMBOL;
		oblist = cons(x, oblist);
		return x;
	}
}

/* get extra atom */
pointer mk_extra()
{
	pointer x = get_cell(NIL, NIL);

	type(x) = T_EXTRA | T_ATOM;
	return x;
}

/* get number atom */
pointer mk_integer(int num)
{
	pointer x = get_cell(NIL, NIL);

	type(x) = T_NUMBER | T_ATOM;
	ivalue(x) = num;
	return x;
}

/* get number atom */
void mk_filetype(pointer x, FILE *fp)
{
	type(x) = T_FILE | T_ATOM;
	fpvalue(x) = fp;
}

/* make symbol or number atom from string */
pointer mk_atom(char   *q)
{
	char    c, *p;

	p = q;
	if (!isdigit(c = *p++)) {
		if ((c != '+' && c != '-') || !isdigit(*p))
			return mk_symbol(q);
	}
	for ( ; (c = *p) != 0; ++p)
		if (!isdigit(c))
			return mk_symbol(q);
	return mk_integer(atoi(q));
}

/* make constant */
pointer mk_const(char *name)
{
	int    x;
	char   buf[20];

	if (!strcmp(name, "t"))
		return T;
	else if (!strcmp(name, "f"))
		return F;
	else if (*name == 'o') {/* #o (octal) */
		sprintf(buf, "0%s", &name[1]);
		sscanf(buf, "%o", &x);
		return mk_integer(x);
	} else if (*name == 'd') {	/* #d (decimal) */
		sscanf(&name[1], "%d", &x);
		return mk_integer(x);
	} else if (*name == 'x') {	/* #x (hex) */
		sprintf(buf, "0x%s", &name[1]);
		sscanf(buf, "%x", &x);
		return mk_integer(x);
	} else
		return NIL;
}

void init_vars_global()
{
	pointer x;

	/* init input/output file */
	infp = 0;
	args = 0;

	type(NIL) = (T_ATOM | MARK);
	car(NIL) = cdr(NIL) = NOP;
	type(NOP) = (T_ATOM | MARK);
	car(NOP) = cdr(NOP) = NOP;  // so never a crash
	type(T) = (T_ATOM | MARK);
	car(T) = cdr(T) = T;
	type(F) = (T_ATOM | MARK);
	car(F) = cdr(F) = F;

	/* init global_env */
	global_env = cons(NIL, NIL);
	/* init else */
	x = mk_symbol("else");
	car(global_env) = cons(cons(x, T), car(global_env));
}

void mk_syntax(int op, char *name)
{
	pointer x;

	x = cons(mk_string(name), NIL);
	type(x) = (T_SYNTAX | T_SYMBOL);
	syntaxnum(x) = op;
	oblist = cons(x, oblist);
}

void init_syntax()
{
	/* init syntax */
	mk_syntax(OP_LAMBDA, "lambda");
	mk_syntax(OP_QUOTE, "quote");
	mk_syntax(OP_DEF0, "define");
	mk_syntax(OP_IF0, "if");
	mk_syntax(OP_BEGIN, "begin");
	mk_syntax(OP_SET0, "set!");
	mk_syntax(OP_LET0, "let");
	mk_syntax(OP_LET0AST, "let*");
	mk_syntax(OP_LET0REC, "letrec");
	mk_syntax(OP_COND0, "cond");
	mk_syntax(OP_DELAY, "delay");
	mk_syntax(OP_AND0, "and");
	mk_syntax(OP_OR0, "or");
	mk_syntax(OP_C0STREAM, "cons-stream");
	mk_syntax(OP_0MACRO, "macro");
	mk_syntax(OP_CASE0, "case");
}


void mk_proc(int op, char *name)
{
	pointer x, y;

	x = mk_symbol(name);
	y = get_cell(NIL, NIL);
	type(y) = (T_PROC | T_ATOM);
	ivalue(y) = op;
	car(global_env) = cons(cons(x, y), car(global_env));
}

void init_procs()
{
	/* init procedure */
	mk_proc(OP_PEVAL, "eval");
	mk_proc(OP_PAPPLY, "apply");
	mk_proc(OP_CONTINUATION, "call/cc");
	mk_proc(OP_FORCE, "force");
	mk_proc(OP_CAR, "car");
	mk_proc(OP_CDR, "cdr");
	mk_proc(OP_CONS, "cons");
	mk_proc(OP_SETCAR, "set-car!");
	mk_proc(OP_SETCDR, "set-cdr!");
	mk_proc(OP_ADD, "+");
	mk_proc(OP_SUB, "-");
	mk_proc(OP_MUL, "*");
	mk_proc(OP_DIV, "/");
	mk_proc(OP_REM, "remainder");
	mk_proc(OP_NOT, "not");
	mk_proc(OP_BOOL, "boolean?");
	mk_proc(OP_SYMBOL, "symbol?");
	mk_proc(OP_NUMBER, "number?");
	mk_proc(OP_STRING, "string?");
	mk_proc(OP_PROC, "procedure?");
	mk_proc(OP_PAIR, "pair?");
	mk_proc(OP_EQV, "eqv?");
	mk_proc(OP_EQ, "eq?");
	mk_proc(OP_NULL, "null?");
	mk_proc(OP_ZEROP, "zero?");
	mk_proc(OP_POSP, "positive?");
	mk_proc(OP_NEGP, "negative?");
	mk_proc(OP_NEQ, "=");
	mk_proc(OP_LESS, "<");
	mk_proc(OP_GRE, ">");
	mk_proc(OP_LEQ, "<=");
	mk_proc(OP_GEQ, ">=");
	mk_proc(OP_READ, "read");
	mk_proc(OP_WRITE, "write");
	mk_proc(OP_DISPLAY, "display");
	mk_proc(OP_NEWLINE, "newline");
	mk_proc(OP_LOAD, "load");
	mk_proc(OP_ERR0, "error");
	mk_proc(OP_REVERSE, "reverse");
	mk_proc(OP_APPEND, "append");
	mk_proc(OP_PUT, "put");
	mk_proc(OP_GET, "get");
	mk_proc(OP_GC, "gc");
	mk_proc(OP_GCVERB, "gc-verbose");
	mk_proc(OP_NEWSEGMENT, "new-segment");
	mk_proc(OP_LIST_LENGTH, "length");	/* a.k */
	mk_proc(OP_LIST_REF, "list-ref");	/* w.b */
	mk_proc(OP_ASSQ, "assq");	/* a.k */
	mk_proc(OP_GET_CLOSURE, "get-closure-code");	/* a.k */
	mk_proc(OP_CLOSUREP, "closure?");	/* a.k */
	mk_proc(OP_MACROP, "macro?");	/* a.k */
	mk_proc(OP_QUIT, "quit");
    mk_proc(OP_LIST_UNDEF, "list>undef"); // w.b.
    mk_proc(OP_LIST_INT, "list>int");
    mk_proc(OP_LIST_LIST, "list>list");
}

/* initialize several globals */
void init_globals()
{
	init_vars_global();
	init_syntax();
	init_procs();
	/* intialization of global pointers to special symbols */
	LAMBDA = mk_symbol("lambda");
	QUOTE = mk_symbol("quote");
	QQUOTE = mk_symbol("quasiquote");
	UNQUOTE = mk_symbol("unquote");
	UNQUOTESP = mk_symbol("unquote-splicing");
}

/* initialization of Mini-Scheme */
void init_scheme()
{
	pointer i;

	if (alloc_cellseg(FIRST_CELLSEGS) != FIRST_CELLSEGS)
		FatalError("Unable to allocate initial cell segments");
	init_globals();
}





/* ========== Rootines for Reading ========== */

enum token_code {
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_DOT,
	TOK_ATOM,
	TOK_QUOTE,
	TOK_COMMENT,
	TOK_DQUOTE,
	TOK_BQUOTE,
	TOK_COMMA,
	TOK_ATMARK,
	TOK_SHARP
};

#define LINESIZE 1000
char    linebuff[LINESIZE];
char    strbuf[200];
char   *currentline,
       *endline;

void back_to_interactive() {  // after error
  if (infp && infp!=stdin)
    fclose(infp);
  for (; file_args != NIL; file_args = cdr(file_args))
    if (isfile(car(file_args)) && infp!=fpvalue(car(file_args)))
      fclose(fpvalue(car(file_args)));
  infp = stdin;
  currentline = endline = 0;
  longjmp(error_jmp,OP_T0LVL);
}

/* get new character from input file */
int inchar()
{ char *arg;
  if (currentline >= endline) {    /* input buffer is empty */
    if (!infp || feof(infp)) {
      if (infp && infp!=stdin) {
        fclose(infp);
        file_args = cdr(file_args);
      }
      if (file_args!=NIL) {
		if (isstring(car(file_args))) {
          arg=strvalue(car(file_args));
          if ((infp = fopen(arg, "r")) == NULL) {
            printf("Error: unable to open %s\n", strvalue(car(file_args)));
            back_to_interactive();
          }
          else {
			mk_filetype(car(file_args),infp);
            printf("Loading %s\n",arg);
          }
        }
        else if (isfile(car(file_args)))
          infp=fpvalue(car(file_args));
      }
      else
        infp = stdin;
    }
    if (infp==stdin) {
      currentline = get_line();
      putchar('\n');
    }
    else {
      strcpy(linebuff,"\n");
      fgets(currentline = linebuff, LINESIZE, infp);
    }
    endline = currentline + strlen(currentline);
  }
  return *currentline++;
}

/* check c is delimiter */
bool isdelim( char   *s, char    c)
{
	while (*s)
		if (*s++ == c)
			return 0;
	return 1;
}

/* read characters to delimiter */
char   *readstr(char   *delim)
{
	char *p = strbuf,
         ch;

	while (isdelim(delim, ch = inchar())) *p++=ch; 
	--currentline;
	*p = 0;
	return strbuf;
}

/* read string expression "xxx...xxx" */
char   *readstrexp()
{
	char    c, *p = strbuf;

	for (;;) {
		if ((c = inchar()) != '"')
			*p++ = c;
		else if (p > strbuf && p[-1] == '\\')
			p[-1] = '"';
		else {
			*p = 0;
			return strbuf;
		}
	}
}

/* get token */
int token()
{
	char ch;
	do {
      ch=inchar();
    } while (isspace(ch));
	switch (ch) {
	case '(':
		return TOK_LPAREN;
	case ')':
		return TOK_RPAREN;
	case '.':
		return TOK_DOT;
	case '\'':
		return TOK_QUOTE;
	case ';':
		return TOK_COMMENT;
	case '"':
		return TOK_DQUOTE;
	case '`':
		return TOK_BQUOTE;
	case ',':
		if (inchar() == '@')
			return TOK_ATMARK;
		else {
			--currentline;
			return TOK_COMMA;
		}
	case '#':
		return TOK_SHARP;
	default:
		--currentline;
		return TOK_ATOM;
	}
}

/* ========== Rootines for Printing ========== */
#define ok_abbrev(x) (ispair(x) && cdr(x) == NIL)

/* print atoms */
int printatom(pointer l, int f)
{
  char *p=0, *s;
  
  if (l == NOP)
    p = 0;
  else if (l == NIL)
    p = "()";
  else if (l == T)
    p = "#t";
  else if (l == F)
    p = "#f";
  else if (isnumber(l)) {
    p = strbuf;
    sprintf(p, "%d", ivalue(l));
  }
  else if (isstring(l)) {
    if (!f) {
      p = strbuf;
      for (s=strvalue(l); *s; ++s) {
        if (*s=='\\') {
          switch (s[1]) {
            case 'n': *p++ = '\n'; ++s; break;
            case '\\': *p++ = '\\'; ++s; break;
            default: ++s;
          }
        }
        else
          *p++ = *s;
      }
      *p = 0;
    }
    else {
      p = strbuf;
      *p++ = '"';
      for (s=strvalue(l); *s; ++s) *p++ = *s;
      *p++ = '"'; *p = 0;
    }
    p = strbuf;
  }
  else if (issymbol(l))
    p = symname(l);
  else if (isproc(l)) {
    p = strbuf;
    sprintf(p, "#<PROCEDURE %d>", procnum(l));
  }
  else if (ismacro(l)) {
    p = "#<MACRO>";
  }
  else if (isclosure(l))
    p = "#<CLOSURE>";
  else if (iscontinuation(l))
    p = "#<CONTINUATION>";
  else if (isextra(l))  // function list>undef
    p = 0;
  else
    p = "#<error>";
  if (f < 0)
    return strlen(p);
  if (p) fputs(p, stdout);
  return 0;
}


/* ========== Rootines for Evaluation Cycle ========== */

/* make closure. c is code. e is environment */
pointer mk_closure(pointer c, pointer e)
{
	pointer x = get_cell(c, e);

	type(x) = T_CLOSURE;
	car(x) = c;
	cdr(x) = e;
	return x;
}

/* make continuation. */
pointer mk_continuation(pointer d)
{
	pointer x = get_cell(NIL, d);

	type(x) = T_CONTINUATION;
	cont_dump(x) = d;
	return x;
}

/* reverse list -- make new cells */
pointer reverse(pointer a)		/* a must be checked by gc */
{
	pointer p = NIL;

	for ( ; ispair(a); a = cdr(a))
		p = cons(car(a), p);
	return p;
}

/* reverse list --- no make new cells */
pointer non_alloc_rev(pointer term, pointer list)
{
	pointer p = list, result = term, q;

	while (p != NIL) {
		q = cdr(p);
		cdr(p) = result;
		result = p;
		p = q;
	}
	return result;
}

/* append list -- make new cells */
pointer append(pointer a, pointer b)
{
	pointer p = b, q;

	if (a != NIL) {
		a = reverse(a);
		while (a != NIL) {
			q = cdr(a);
			cdr(a) = p;
			p = a;
			a = q;
		}
	}
	return p;
}

/* equivalence of atoms */
bool eqv(pointer a, pointer b)
{
	if (isstring(a)) {
		if (isstring(b))
			return strvalue(a) == strvalue(b);
		else
			return 0;
	} else if (isnumber(a)) {
		if (isnumber(b))
			return ivalue(a) == ivalue(b);
		else
			return 0;
	} else
		return a == b;
}

/* true or false value macro */
#define istrue(p)       ((p) != NIL && (p) != F)
#define isfalse(p)      ((p) == NIL || (p) == F)

pointer Error_0(char *s) {
    args = cons(mk_string(s), NIL);
    oper = OP_ERR0;
    return T;
}

pointer Error_1(char *s, pointer a) {
    args = cons((a), NIL);
    args = cons(mk_string(s), args);
    oper = OP_ERR0;
    return T;
}

void save(int op, pointer b, pointer c) {
    dump = cons(envir, cons(c, dump));
    dump = cons(b, dump);
    dump = cons(mk_integer(op), dump);
}

pointer save_return(pointer a) {
    value = a;
    oper = ivalue(car(dump));
    args = cadr(dump);
    envir = caddr(dump);
    code = cadddr(dump);
    dump = cddddr(dump);
    return T;
}

#define s_retbool(tf)	return save_return((tf) ? T : F)

/* ========== Evaluation Cycle ========== */

pointer opexe(int op)
{
	pointer x, y;
	int v;
start:
	switch (op) {
	case OP_LOAD:		/* load */
        if (args) {
			if (!isstring(car(args)))
				return Error_0("load: argument is not string");
			file_args=cons(mk_string(strvalue(car(args))),file_args);
		}
		currentline=endline=0;
		infp=0;
		op = OP_T0LVL; goto start;

	case OP_T0LVL:	/* top level */
		dump = NIL;
		envir = global_env;
		save(OP_VALUEPRINT, NIL, NIL);
		save(OP_T1LVL, NIL, NIL);
		if (infp == stdin)
			putchar('\n');
		op = OP_READ; goto start;

	case OP_T1LVL:	/* top level */
		code = value;
		op = OP_EVAL; goto start;
		
	case OP_READ:		/* read */
		tok = token();
		op = OP_RDSEXPR; goto start;

	case OP_VALUEPRINT:	/* print evaluation result */
		print_flag = 1;
		args = value;
		save(OP_T0LVL, NIL, NIL);
		op = OP_P0LIST; goto start;

	case OP_EVAL:		/* main part of evaluation */
		if (issymbol(code)) {	/* symbol */
			for (x = envir; x != NIL; x = cdr(x)) {
				for (y = car(x); y != NIL; y = cdr(y))
					if (caar(y) == code)
						break;
				if (y != NIL)
					break;
			}
			if (x != NIL) {
				return save_return(cdar(y));
			} else {
				return Error_1("unbounded variable:", code);
			}
		} else if (ispair(code)) {
			if (issyntax(x = car(code))) {	/* SYNTAX */
				code = cdr(code);
				op = syntaxnum(x); goto start;
			} else {/* first, eval top element and eval arguments */
				save(OP_E0ARGS, NIL, code);
				code = car(code);
				op = OP_EVAL; goto start;
			}
		} else
			return save_return(code);

	case OP_E0ARGS:	/* eval arguments */
		if (ismacro(value)) {	/* macro expansion */
			save(OP_DOMACRO, NIL, NIL);
			args = cons(code, NIL);
			code = value;
			op = OP_APPLY; goto start;
		} else {
			code = cdr(code);
			op = OP_E1ARGS; goto start;
		}

	case OP_E1ARGS:	/* eval arguments */
		args = cons(value, args);
		if (ispair(code)) {	/* continue */
			save(OP_E1ARGS, args, cdr(code));
			code = car(code);
			args = NIL;
			op = OP_EVAL; goto start;
		} else {	/* end */
			args = reverse(args);
			code = car(args);
			args = cdr(args);
			op = OP_APPLY; goto start;
		}

	case OP_APPLY:		/* apply 'code' to 'args' */
		if (isproc(code)) {
			op = procnum(code);	goto start; /* PROCEDURE */
		} else if (isclosure(code)) {	    /* CLOSURE */
			/* make environment */
			envir = cons(NIL, closure_env(code));
			for (x = car(closure_code(code)), y = args;
			     ispair(x); x = cdr(x), y = cdr(y)) {
				if (y == NIL) {
					return Error_0("few arguments");
				} else {
					car(envir) = cons(cons(car(x), car(y)), car(envir));
				}
			}
			if (x == NIL);
			else if (issymbol(x))
				car(envir) = cons(cons(x, y), car(envir));
			else {
				return Error_0("syntax error in closure");
			}
			code = cdr(closure_code(code));
			args = NIL;
			op = OP_BEGIN; goto start;
		} else if (iscontinuation(code)) {	/* CONTINUATION */
			dump = cont_dump(code);
			return save_return(args != NIL ? car(args) : NIL);
		}
		else 
			return Error_1("illegal function:",code);

	case OP_DOMACRO:	/* do macro */
		code = value;
		op = OP_EVAL; goto start;

	case OP_LAMBDA:	/* lambda */
		return save_return(mk_closure(code, envir));

	case OP_QUOTE:		/* quote */
		return save_return(car(code));

	case OP_DEF0:	/* define */
		if (ispair(car(code))) {
			x = caar(code);
			code = cons(LAMBDA, cons(cdar(code), cdr(code)));
		} else {
			x = car(code);
			code = cadr(code);
		}
		if (!issymbol(x)) {
			return Error_1("variable is not symbol:",x);
		}
		save(OP_DEF1, NIL, x);
		op = OP_EVAL; goto start;

	case OP_DEF1:	/* define */
		for (x = car(envir); x != NIL; x = cdr(x))
			if (caar(x) == code)
				break;
		if (x != NIL)
			cdar(x) = value;
		else
			car(envir) = cons(cons(code, value), car(envir));
		return save_return(NOP);
		//return save_return(code);

	case OP_SET0:		/* set! */
		save(OP_SET1, NIL, car(code));
		code = cadr(code);
		op = OP_EVAL; goto start;

	case OP_SET1:		/* set! */
		for (x = envir; x != NIL; x = cdr(x)) {
			for (y = car(x); y != NIL; y = cdr(y))
				if (caar(y) == code)
					break;
			if (y != NIL)
				break;
		}
		if (x != NIL) {
			cdar(y) = value;
			return save_return(NOP);
		}
		return Error_1("unbounded variable:", code);

	case OP_BEGIN:		/* begin */
		if (!ispair(code))
			return save_return(code);
		if (cdr(code) != NIL)
			save(OP_BEGIN, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_IF0:		/* if */
		save(OP_IF1, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_IF1:		/* if */
		if (istrue(value))
			code = car(code);
		else
			code = cadr(code);	/* (if #f 1) ==> () because * car(NIL) = NIL */
		op = OP_EVAL; goto start;

	case OP_LET0:		/* let */
		args = NIL;
		value = code;
		code = issymbol(car(code)) ? cadr(code) : car(code);
		op = OP_LET1; goto start;

	case OP_LET1:		/* let (caluculate parameters) */
		args = cons(value, args);
		if (ispair(code)) {	/* continue */
			save(OP_LET1, args, cdr(code));
			code = cadar(code);
			args = NIL;
			op = OP_EVAL; goto start;
		} else {	/* end */
			args = reverse(args);
			code = car(args);
			args = cdr(args);
			op = OP_LET2; goto start;
		}

	case OP_LET2:		/* let */
		envir = cons(NIL, envir);
		for (x = issymbol(car(code)) ? cadr(code) : car(code), y = args;
		     y != NIL; x = cdr(x), y = cdr(y))
			car(envir) = cons(cons(caar(x), car(y)), car(envir));
		if (issymbol(car(code))) {	/* named let */
			for (x = cadr(code), args = NIL; x != NIL; x = cdr(x))
				args = cons(caar(x), args);
			x = mk_closure(cons(reverse(args), cddr(code)), envir);
			car(envir) = cons(cons(car(code), x), car(envir));
			code = cddr(code);
			args = NIL;
		} else {
			code = cdr(code);
			args = NIL;
		}
		op = OP_BEGIN; goto start;

	case OP_LET0AST:	/* let* */
		if (car(code) == NIL) {
			envir = cons(NIL, envir);
			code = cdr(code);
			op = OP_BEGIN; goto start;
		}
		save(OP_LET1AST, cdr(code), car(code));
		code = cadaar(code);
		op = OP_EVAL; goto start;

	case OP_LET1AST:	/* let* (make new frame) */
		envir = cons(NIL, envir);
		op = OP_LET2AST; goto start;

	case OP_LET2AST:	/* let* (caluculate parameters) */
		car(envir) = cons(cons(caar(code), value), car(envir));
		code = cdr(code);
		if (ispair(code)) {	/* continue */
			save(OP_LET2AST, args, code);
			code = cadar(code);
			args = NIL;
			op = OP_EVAL; goto start;
		} else {	/* end */
			code = args;
			args = NIL;
			op = OP_BEGIN; goto start;
		}
	case OP_LET0REC:	/* letrec */
		envir = cons(NIL, envir);
		args = NIL;
		value = code;
		code = car(code);
		op = OP_LET1REC; goto start;

	case OP_LET1REC:	/* letrec (caluculate parameters) */
		args = cons(value, args);
		if (ispair(code)) {	/* continue */
			save(OP_LET1REC, args, cdr(code));
			code = cadar(code);
			args = NIL;
			op = OP_EVAL; goto start;
		} else {	/* end */
			args = reverse(args);
			code = car(args);
			args = cdr(args);
			op = OP_LET2REC; goto start;
		}

	case OP_LET2REC:	/* letrec */
		for (x = car(code), y = args; y != NIL; x = cdr(x), y = cdr(y))
			car(envir) = cons(cons(caar(x), car(y)), car(envir));
		code = cdr(code);
		args = NIL;
		op = OP_BEGIN; goto start;

	case OP_COND0:		/* cond */
		if (!ispair(code)) {
			return Error_0("syntax error in cond");
		}
		save(OP_COND1, NIL, code);
		code = caar(code);
		op = OP_EVAL; goto start;

	case OP_COND1:		/* cond */
		if (istrue(value)) {
			if ((code = cdar(code)) == NIL)
				return save_return(value);
			op = OP_BEGIN; goto start;
		} else {
			if ((code = cdr(code)) == NIL)
				return save_return(NIL);
			save(OP_COND1, NIL, code);
			code = caar(code);
			op = OP_EVAL; goto start;
		}

	case OP_DELAY:		/* delay */
		x = mk_closure(cons(NIL, code), envir);
		setpromise(x);
		return save_return(x);

	case OP_AND0:		/* and */
		if (code == NIL)
			return save_return(T);
		save(OP_AND1, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_AND1:		/* and */
		if (isfalse(value))
			return save_return(value);
		if (code == NIL)
			return save_return(value);
		save(OP_AND1, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_OR0:		/* or */
		if (code == NIL)
			return save_return(F);
		save(OP_OR1, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_OR1:		/* or */
		if (istrue(value))
			return save_return(value);
		if (code == NIL)
			return save_return(value);
		save(OP_OR1, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_C0STREAM:	/* cons-stream */
		save(OP_C1STREAM, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_C1STREAM:	/* cons-stream */
		args = value;	/* save value to register args for gc */
		x = mk_closure(cons(NIL, code), envir);
		setpromise(x);
		return save_return(cons(args, x));

	case OP_0MACRO:	/* macro */
		x = car(code);
		code = cadr(code);
		if (!issymbol(x)) {
			return Error_0("variable is not symbol");
		}
		save(OP_1MACRO, NIL, x);
		op = OP_EVAL; goto start;

	case OP_1MACRO:	/* macro */
		type(value) |= T_MACRO;
		for (x = car(envir); x != NIL; x = cdr(x))
			if (caar(x) == code)
				break;
		if (x != NIL)
			cdar(x) = value;
		else
			car(envir) = cons(cons(code, value), car(envir));
		return save_return(NOP);

	case OP_CASE0:		/* case */
		save(OP_CASE1, NIL, cdr(code));
		code = car(code);
		op = OP_EVAL; goto start;

	case OP_CASE1:		/* case */
		for (x = code; x != NIL; x = cdr(x)) {
			if (!ispair(y = caar(x)))
				break;
			for ( ; y != NIL; y = cdr(y))
				if (eqv(car(y), value))
					break;
			if (y != NIL)
				break;
		}
		if (x != NIL) {
			if (ispair(caar(x))) {
				code = cdar(x);
				op = OP_BEGIN; goto start;
			} else {    /* else */
				save(OP_CASE2, NIL, cdar(x));
				code = caar(x);
				op = OP_EVAL; goto start;
			}
		} else
			return save_return(NIL);

	case OP_CASE2:		/* case */
		if (istrue(value)) {
			op = OP_BEGIN; goto start;
		}
		return save_return(NIL);

	case OP_PAPPLY:	/* apply */
		code = car(args);
		args = cadr(args);
		op = OP_APPLY; goto start;

	case OP_PEVAL:	/* eval */
		code = car(args);
		args = NIL;
		op = OP_EVAL; goto start;

	case OP_CONTINUATION:	/* call-with-current-continuation */
		code = car(args);
		args = cons(mk_continuation(dump), NIL);
		op = OP_APPLY; goto start;

	case OP_ADD:		/* + */
		for (x = args, v = 0; x != NIL; x = cdr(x))
			v += ivalue(car(x));
		return save_return(mk_integer(v));

	case OP_SUB:		/* - */
		if (cdr(args)==NIL) {
			x = args;
			v = 0;
		} else {
			x = cdr(args);
			v = ivalue(car(args));
		} 
		for (; x != NIL; x = cdr(x))
			v -= ivalue(car(x));
		return save_return(mk_integer(v));

	case OP_MUL:		/* * */
		for (x = args, v = 1; x != NIL; x = cdr(x))
			v *= ivalue(car(x));
		return save_return(mk_integer(v));

	case OP_DIV:		/* / */
		for (x = cdr(args), v = ivalue(car(args)); x != NIL; x = cdr(x)) {
			if (ivalue(car(x)) != 0)
				v /= ivalue(car(x));
			else {
				return Error_0("divided by zero");
			}
		}
		return save_return(mk_integer(v));

	case OP_REM:		/* remainder */
		for (x = cdr(args), v = ivalue(car(args)); x != NIL; x = cdr(x)) {
			if (ivalue(car(x)) != 0)
				v %= ivalue(car(x));
			else {
				return Error_0("divided by zero");
			}
		}
		return save_return(mk_integer(v));

	case OP_CAR:		/* car */
		if (ispair(car(args)))
			return save_return(caar(args));
		return Error_0("unable to car for non-cons cell");

	case OP_CDR:		/* cdr */
		if (ispair(car(args)))
			return save_return(cdar(args));
		return Error_0("unable to cdr for non-cons cell");

	case OP_CONS:		/* cons */
		cdr(args) = cadr(args);
		return save_return(args);

	case OP_SETCAR:	/* set-car! */
		if (ispair(car(args))) {
			caar(args) = cadr(args);
			return save_return(NOP);
		}
		return Error_0("unable to set-car! for non-cons cell");

	case OP_SETCDR:	/* set-cdr! */
		if (ispair(car(args))) {
			cdar(args) = cadr(args);
			return save_return(car(args));
		} else {
			return Error_0("unable to set-cdr! for non-cons cell");
		}
	case OP_NOT:		/* not */
		s_retbool(isfalse(car(args)));
	case OP_BOOL:		/* boolean? */
		s_retbool(car(args) == F || car(args) == T);
	case OP_NULL:		/* null? */
		s_retbool(car(args) == NIL);
	case OP_ZEROP:		/* zero? */
		s_retbool(ivalue(car(args)) == 0);
	case OP_POSP:		/* positive? */
		s_retbool(ivalue(car(args)) > 0);
	case OP_NEGP:		/* negative? */
		s_retbool(ivalue(car(args)) < 0);
	case OP_NEQ:		/* = */
		s_retbool(ivalue(car(args)) == ivalue(cadr(args)));
	case OP_LESS:		/* < */
		s_retbool(ivalue(car(args)) < ivalue(cadr(args)));
	case OP_GRE:		/* > */
		s_retbool(ivalue(car(args)) > ivalue(cadr(args)));
	case OP_LEQ:		/* <= */
		s_retbool(ivalue(car(args)) <= ivalue(cadr(args)));
	case OP_GEQ:		/* >= */
		s_retbool(ivalue(car(args)) >= ivalue(cadr(args)));
	case OP_SYMBOL:	/* symbol? */
		s_retbool(issymbol(car(args)));
	case OP_NUMBER:	/* number? */
		s_retbool(isnumber(car(args)));
	case OP_STRING:	/* string? */
		s_retbool(isstring(car(args)));
	case OP_PROC:		/* procedure? */
		/*--
	         * continuation should be procedure by the example
	         * (call-with-current-continuation procedure?) ==> #t
                 * in R^3 report sec. 6.9
	         */
		s_retbool(isproc(car(args)) || isclosure(car(args))
			  || iscontinuation(car(args)));
	case OP_PAIR:		/* pair? */
		s_retbool(ispair(car(args)));
	case OP_EQ:		/* eq? */
		s_retbool(car(args) == cadr(args));
	case OP_EQV:		/* eqv? */
		s_retbool(eqv(car(args), cadr(args)));
	case OP_FORCE:		/* force */
		code = car(args);
		if (ispromise(code)) {
			args = NIL;
			op = OP_APPLY; goto start;
		}
		return save_return(code);

	case OP_WRITE:		/* write */
		print_flag = 1;
		args = car(args);
		op = OP_P0LIST; goto start;

	case OP_DISPLAY:	/* display */
		print_flag = 0;
		args = car(args);
		op = OP_P0LIST; goto start;

	case OP_NEWLINE:	/* newline */
		putchar('\n');
		return save_return(NOP);

	case OP_ERR0:	/* error */
		if (!isstring(car(args))) {
			return Error_0("error -- first argument must be string");
		}
		fputs("Error: ",stdout);
		fputs(strvalue(car(args)),stdout);
		args = cdr(args);
		op = OP_ERR1; goto start;

	case OP_ERR1:	/* error */
		fputs(" ",stdout);
		if (args != NIL) { 
			save(OP_ERR1, cdr(args), NIL);
			args = car(args);
			print_flag = 1;
			op = OP_P0LIST; goto start;
		}
		putchar('\n');
		back_to_interactive();

	case OP_REVERSE:	/* reverse */
		return save_return(reverse(car(args)));

	case OP_APPEND:	/* append */
		if (args==NIL)
			return save_return(NIL);
		x=car(args);
		if (cdr(args)==NIL)
			return save_return(args);
		for (y = cdr(args); y != NIL; y = cdr(y))
			x=append(x,car(y));
		return save_return(x);

	case OP_PUT:		/* put */
		if (!hasprop(car(args)) || !hasprop(cadr(args))) {
			return Error_0("illegal use of put");
		}
		for (x = symprop(car(args)), y = cadr(args); x != NIL; x = cdr(x))
			if (caar(x) == y)
				break;
		if (x != NIL)
			cdar(x) = caddr(args);
		else
			symprop(car(args)) = cons(cons(y, caddr(args)),
						  symprop(car(args)));
		return save_return(T);

	case OP_GET:		/* get */
		if (!hasprop(car(args)) || !hasprop(cadr(args))) {
			return Error_0("illegal use of get");
		}
		for (x = symprop(car(args)), y = cadr(args); x != NIL; x = cdr(x))
			if (caar(x) == y)
				break;
		if (x != NIL)
			return save_return(cdar(x));
		return save_return(NIL);

	case OP_QUIT:		/* quit */
		return NIL;

	case OP_GC:		/* gc */
		gc(NIL, NIL);
		return save_return(T);

	case OP_GCVERB:		/* gc-verbose */
	{	int	was = gc_verbose;
		
		gc_verbose = (car(args) != F);
		s_retbool(was);
	}

	case OP_NEWSEGMENT:	/* new-segment */
		if (!isnumber(car(args))) {
			return Error_0("new-segment -- argument must be number");
		}
		v = ivalue(car(args));
		if (v > alloc_cellseg(v) && gc_verbose)
			printf("(that's less then %d)\n",v);
		return save_return(NOP);

	/* ========== reading part ========== */
	case OP_RDSEXPR:
		switch (tok) {
		case TOK_COMMENT:
			while (inchar() != '\n');
			tok = token();
			op = OP_RDSEXPR; goto start;

		case TOK_LPAREN:
			tok = token();
			if (tok == TOK_RPAREN)
				return save_return(NIL);
			if (tok == TOK_DOT)
				return Error_0("syntax error -- illegal dot expression");
			save(OP_RDLIST, NIL, NIL);
			op = OP_RDSEXPR; goto start;

		case TOK_QUOTE:
			save(OP_RDQUOTE, NIL, NIL);
			tok = token();
			op = OP_RDSEXPR; goto start;

		case TOK_BQUOTE:
			save(OP_RDQQUOTE, NIL, NIL);
			tok = token();
			op = OP_RDSEXPR; goto start;
		case TOK_COMMA:
			save(OP_RDUNQUOTE, NIL, NIL);
			tok = token();
			op = OP_RDSEXPR; goto start;
		case TOK_ATMARK:
			save(OP_RDUQTSP, NIL, NIL);
			tok = token();
			op = OP_RDSEXPR; goto start;
		case TOK_ATOM:
			return save_return(mk_atom(readstr("();\t\n ")));
		case TOK_DQUOTE:
			return save_return(mk_string(readstrexp()));
		case TOK_SHARP:
			if ((x = mk_const(readstr("();\t\n "))) == NIL)
				return Error_0("undefined sharp expression");
			return save_return(x);

		default:
            sprintf(strbuf,"syntax error -- illegal token (code: %d)",tok);
			return Error_0(strbuf);
		}
		break;

	case OP_RDLIST:
		args = cons(value, args);
		tok = token();
		if (tok == TOK_COMMENT) {
			while (inchar() != '\n');
			tok = token();
		}
		if (tok == TOK_RPAREN)
			return save_return(non_alloc_rev(NIL, args));
		if (tok == TOK_DOT) {
			save(OP_RDDOT, args, NIL);
			tok = token();
			op = OP_RDSEXPR; goto start;
		}
		save(OP_RDLIST, args, NIL);;
		op = OP_RDSEXPR; goto start;

	case OP_RDDOT:
		if (token() != TOK_RPAREN) {
			return Error_0("syntax error -- illegal dot expression");
		} else
			return save_return(non_alloc_rev(value, args));

	case OP_RDQUOTE:
		return save_return(cons(QUOTE, cons(value, NIL)));

	case OP_RDQQUOTE:
		return save_return(cons(QQUOTE, cons(value, NIL)));

	case OP_RDUNQUOTE:
		return save_return(cons(UNQUOTE, cons(value, NIL)));

	case OP_RDUQTSP:
		return save_return(cons(UNQUOTESP, cons(value, NIL)));

	/* ========== printing part ========== */
	case OP_P0LIST:
		if (!ispair(args)) {
			printatom(args, print_flag);
			return save_return(NOP);
		}
		if (car(args) == QUOTE && ok_abbrev(cdr(args))) {
			fputs("'",stdout);
			args = cadr(args);
			op = OP_P0LIST; goto start;
		}
		if (car(args) == QQUOTE && ok_abbrev(cdr(args))) {
			fputs("`",stdout);
			args = cadr(args);
			op = OP_P0LIST; goto start;
		} 
		if (car(args) == UNQUOTE && ok_abbrev(cdr(args))) {
			fputs(",",stdout);
			args = cadr(args);
			op = OP_P0LIST; goto start;
		} 
		if (car(args) == UNQUOTESP && ok_abbrev(cdr(args))) {
			fputs(",@",stdout);
			args = cadr(args);
			op = OP_P0LIST; goto start;
		}
		fputs("(",stdout);
		save(OP_P1LIST, cdr(args), NIL);
		args = car(args);
		op = OP_P0LIST; goto start;

	case OP_P1LIST:
		if (ispair(args)) {
			save(OP_P1LIST, cdr(args), NIL);
			printf(" ");
			args = car(args);
			op = OP_P0LIST; goto start;
		}
		if (args != NIL) {
			printf(" . ");
			printatom(args, print_flag);
		}
		printf(")",stdout);
		return save_return(NOP);

	case OP_LIST_LENGTH:	/* list-length */	/* a.k */
		for (x = car(args), v = 0; ispair(x); x = cdr(x))
			++v;
		return save_return(mk_integer(v));
		
	case OP_LIST_REF:	/* list-ref */
        v = ivalue(cadr(args));
		for (x = car(args);; --v, x = cdr(x)) {
            if (!ispair(x)) return Error_0("list ref out of bounds");
			if (v <= 0) break;
		}
		return save_return(car(x));
		
	case OP_ASSQ:		/* assq */	/* a.k */
		x = car(args);
		for (y = cadr(args); ispair(y); y = cdr(y)) {
			if (!ispair(car(y))) {
				return Error_0("unable to handle non pair element");
			}
			if (x == caar(y))
				break;
		}
		if (ispair(y))
			return save_return(car(y));
		return save_return(F);
		
	case OP_GET_CLOSURE:	/* get-closure-code */	/* a.k */
		args = car(args);
		if (args == NIL)
			return save_return(F);
		if (isclosure(args))
			return save_return(cons(LAMBDA, closure_code(value)));
		if (ismacro(args))
			return save_return(cons(LAMBDA, closure_code(value)));
		return save_return(F);
		
	case OP_CLOSUREP:		/* closure? */
		/*
		 * Note, macro object is also a closure.
		 * Therefore, (closure? <#MACRO>) ==> #t
		 */
		if (car(args) == NIL)
		    return save_return(F);
		s_retbool(isclosure(car(args)));
	case OP_MACROP:		/* macro? */
		if (car(args) == NIL)
		    return save_return(F);
		s_retbool(ismacro(car(args)));

    case OP_LIST_UNDEF:
        x=list_to_undef(car(args));
        if (x==T) return x;
        return save_return(x);
    case OP_LIST_INT:
        x=list_to_int(car(args));
        if (x==T) return x;
        return save_return(x);
    case OP_LIST_LIST:
        x=list_to_list(car(args));
        if (x==T) return x;
        return save_return(x);

	default:
		sprintf(strbuf, "illegal operator (value: %d)", oper);
		return Error_0(strbuf);
	}
	return T;	/* NOTREACHED */
}

/* kernel of this intepreter */
pointer Eval_Cycle(int op)
{

	oper = op;
	for (;;)
		if (opexe(oper) == NIL)
			return NIL;
}

/* ========== Main ========== */

int main(int argc,char **argv)
{   char buf[120],
         *p;
    int n,
        op;

	puts(banner);
	init_scheme();
    p=getenv("MINISCHEME_HOME");
    if (p) {
      strncpy(buf,p,100);
      strcat(buf,"/");
      strcat(buf,InitFile);
    }
    else
      strcpy(buf,InitFile);
	file_args = cons(mk_string(buf), NIL);
    for (n=1;n<argc;++n) {
      if (!strcmp(argv[n],"-h")) {
        puts("Usage:");
        puts("  miniscm [file1 file2 ...]");
      }
      else
        file_args=append(file_args,cons(mk_string(argv[n]),NIL));
    }
    setScreen(1);
    op = setjmp(error_jmp); // at startup: op = OP_LOAD
	Eval_Cycle(op);
    setScreen(0);
} 

//========== for external use ===============

#undef car
#undef cdr
#undef cadr
#undef caddr
pointer car(pointer p) { return p->_object._cons._car; }
pointer cdr(pointer p) { return p->_object._cons._cdr; }
pointer cadr(pointer a) { return car(cdr(a)); }
pointer caddr(pointer a) { return car(cdr(cdr(a))); }

pointer nil_pointer() { return NIL; }
bool is_symbol(pointer a) { return issymbol(a); }
char *sym_name(pointer p) { return strvalue(car(p)); }
int int_value(pointer p) { return ivalue(p); }
char *string_value(pointer p) { return strvalue(p); }

int list_length(pointer a) {
     int v;
     pointer x;
     for (x = a, v = 0; ispair(x); x = cdr(x))
       ++v;
     if (x==NIL) return v;
     return -1;
}

pointer list_ref(pointer a, int i) {
     pointer x;
	 for (x = a;; --i, x = cdr(x)) {
        if (!ispair(x)) return Error_0("list ref out of bounds");
		if (i <= 0) break;
     }
	 return(car(x));
}
