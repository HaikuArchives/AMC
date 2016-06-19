#include <stdio.h>
#include <string.h>
#include "str.h"

extern void alert(const char *form,...);

const char* str_ch(const char *s,char ch) { // like strchr(), ch can be 0
  if (!ch) return 0;
  for (;*s;s++) if (*s==ch) return s;
  return 0;
}

Str::Str():ch(0),cmt_ch(0) { s[0]=0; }

Str::Str(const char *str):ch(0),cmt_ch(0) {
  if (str) cpy(str); else s[0]=0;
}

void Str::cpy(const char *str) {
  int i=0;
  if (str)
    for (;str[i];++i) {
      if (i>=str_max) { alert("cpy: buf overflow"); s[str_max-1]=0; return; }
      s[i]=str[i];
    }
  s[i]=0;
}

void Str::cat(const char *str) {
  int i=0,j=0;
  if (str) {
    for (;s[i];++i);
    for (;str[j];++i,++j) {
      if (i>=str_max) { alert("cat: buf overflow"); s[str_max-1]=0; return; }
      s[i]=str[j];
    }
  }
  s[i]=0;
}

char* Str::tos(int i) { sprintf(s,"%d",i); return s; }

void Str::new_ext(const char *ext) {  // new extension
   char *p=strrchr(s,'.');
   if (p) *p=0;
   cat(ext);
}

char *Str::get_dir() {  // xx/yy -> xx
  char *p=strrchr(s,'/');
  if (p) *p=0; else cpy(".");
  return s;
}

char *Str::strip_dir() {  // xx/yy -> yy, non-destructive
  char *p=strrchr(s,'/');
  if (p) return p+1;
  return s;
}

char *Str::get_ext() {
  return strrchr(s,'.');
}

bool Str::operator==(const char* s2) { return !strcmp(s,s2); }

void Str::rword(FILE *in,const char *delim) {
  int i;
  char ch2;
  s[0]=0;
  do {
    ch=getc(in);
    if (ch==EOF) return;
    if (ch==cmt_ch) for(;ch=getc(in),ch!='\n';);
  } while (str_ch(" \t\n",ch));
  for (i=0;!str_ch(delim,ch);++i) {
    if (i>=str_max) { alert("rword: buf overflow"); s[str_max-1]=0; return; }
    s[i]=ch; ch=getc(in);
    if (ch==EOF) break;
    if (ch==cmt_ch) {
      for(;ch=getc(in),ch!='\n';);
      s[i+1]=0; return;
    }
  }
  s[i]=0;
  if (str_ch(" \t",ch)) {
    while (ch2=ch,ch=getc(in)) {
      if (ch==cmt_ch) {
        for (;ch=getc(in),ch!='\n';);
        return;
      }
      if (!str_ch(" \t",ch)) break;
    }
    if (!str_ch(delim,ch)) { ungetc(ch,in); ch=ch2; }
  }
}

void Str::strtok(const char* string,const char* delim,int& tpos) {
  char *p=const_cast<char*>(string),
       *q,
       *stop;
  if (strlen(string) < tpos) { 
    alert("strtok: string=\"%s\", index=%d",string,tpos); return;
  }
  s[0]=0;
  for (p+=tpos;;++p) { 
    if (!*p) {
      ch=0; return;
    }
    if (*p==cmt_ch) {
      for(++p;;++p) {
        if (!*p) { ch=0; return; }
        if (*p=='\n') { tpos=p-string+1; ch='\n'; return; }
      }
    }
    if (!str_ch(" \t",*p)) break;
  }
  q=s;
  for (;*p && !str_ch(delim,*p);p++) {
    *q=*p;
    if (++q>=s+str_max) { alert("strtok: buf overflow"); ch=0; return; }
  }
  *q=0;
  stop=p;
  if (*p) {
    if (str_ch(" \t",*p)) {
      while (str_ch(" \t",*(++p))); 
      if (*p==cmt_ch) {
        for(++p;;++p) {
          if (!*p) { ch=0; return; }
          if (*p=='\n') { tpos=p-string+1; ch='\n'; return; }
        }
      }
      if (*p && !str_ch(delim,*p)) stop=p-1;
      else stop=p;
    }
    tpos = (stop - string) + 1;
  }
  else tpos = p - string;
  ch=*stop;
}

#ifdef TEST
void alert(const char *form,...) { puts(form); }

int main(int argc,char **argv) {
  Str direc(argv[0]);
  printf("dir: %s\n",direc.get_dir());
  FILE *in;
  if ((in=fopen("str.tst","r"))==NULL) exit(1);
  int n,pos;
  Str str;
  str.cmt_ch='#';
  for (;;) {
    str.rword(in," \n;");
    if (str.ch==EOF) { puts("EOF"); break; }
    printf("[%s][%c]\n",str.s,str.ch);
  }
  char tst_str[]="   een ja;nee\n8 # nee dus\n# nee\nend";
  for(pos=0;;) {
    str.strtok(tst_str," \n;",pos);
    printf("[%s][%c]\n",str.s,str.ch);
    if (str.ch==0) { puts("EOL"); break; }
  }
}
#endif
