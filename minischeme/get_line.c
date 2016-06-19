#include <stdio.h>
#include <sys/ioctl.h>
#ifdef LINUX
  #include <termios.h>
  #include <unistd.h>
#endif
#include <signal.h>

#ifndef __cplusplus
typedef char bool;
#endif 
#include "get_line.h"

// Zie: /etc/termcap

enum keynr {
  k_bs=010, k_ret=012, k_esc=033, k_del=0177,
  k_right=500,
  k_left, k_up, k_down, k_ins, k_end, k_pu, k_pd, k_home
}; 

#define cmax 20
#define amax 100
#define nop -0200
char BUF[cmax][amax];
int cur_buf; // current BUF index

#define ctrl(a) (a & 037)

void restoreScreen(int n) { setScreen(0); exit(0); }

void setScreen(bool mod) {
  static struct termios cur_termios,
                 old_termios,
                 *ct;
  if (!ct) {
    ct=&cur_termios;
    tcgetattr(0,ct);
    old_termios = cur_termios;
    signal(SIGINT,restoreScreen);
  }

  if (mod==1) {
    ct->c_lflag &= ~(ICANON|ECHO);
    ct->c_cc[VTIME]=0;
    ct->c_cc[VMIN]=1;  // stdin buffer length
    tcsetattr(0,TCSADRAIN,ct);
  }
  else
    tcsetattr(0,TCSANOW,&old_termios);
}

int get_ch() {
  char ch=getchar();
  if (ch==k_esc) {
    ch=getchar();
    if (ch=='[') {
      ch=getchar();
      switch (ch) {
        case '1': ch=getchar();
          switch (ch) {
            case '~': return k_home; // home
            default: return -1;
          }
        case '4': if (getchar()=='~') return k_end; return -1; // end
        case 'A': return k_up;   // up
        case 'B': return k_down; // down
        case 'D': return k_left; // left
        case 'C': return k_right;// right
        default: return -5;
      }
    }
  }
  switch (ch) {
    case ctrl('P'): return k_up;
    case ctrl('N'): return k_down;
#ifdef LINUX
    case k_del: return k_bs;
#endif
    default: return ch;
  }
}

const char *cm="\033[%d;%dH",   // set cursor absolute position
           *RI="\033[%dC",      // move right
           *LE="\033[%dD",      // move left
           *dl="\033[M";        // delete line

void get_string(char *s,int key,int *stop) {
  char *p;
  int x=0,
      n;
  if (*s!=nop) {
    fputs(dl,stdout);
    fputs(prompt,stdout);
    for (p=s;*p && *p!='\n';++p) putchar(*p);
    x=p-s;
  }
  if (!key) key=get_ch();
  for (p=s+x,*p=0;;key=get_ch()) {
    if (key==k_bs || key==k_del) {  // backspace, delete
      if (key==k_bs) {
        if (x<1) continue;
        printf(LE,1); --x;
      }
      else {  // k_del
        if (x<0 || s[x]=='\n') continue;
      }
      for (p=s+x,n=0;;++p,++n) {
        *p=p[1]; 
        if (!*p || *p=='\n') break;
        putchar(*p);
      }
      putchar(' ');
      printf(LE,n+1);
    }
    else if (key==k_left) {
      if (x>0) { printf(LE,1); --x; }
    }
    else if (key==k_right) {
      if (s[x] && s[x]!='\n') { printf(RI,1); ++x; }
    }
    else if (key>=040 && key<0200) {
      for (p=s+x;*p && *p!='\n';++p); // naar einde s
      if (p-s>=amax-2) { putchar(07); return; }
      p[1]='\n'; // like fgets()
      p[2]=0;
      for (;p>s+x;p--) *p=p[-1];
      *p=key;
      for (n=0;*p && *p!='\n';++p,++n) putchar(*p);
      if (n>1) printf(LE,n-1);
      ++x;
    }
    else if (key==k_ret || key==k_up || key==k_down) {
      *stop=key;
      return;
    }
    else putchar(07);
  } 
}

char *get_line() {  // supposed: setScreen(1) has been called.
  int cs=cur_buf,
      lst_cs,
      key;
  if (--cs<0) cs=cmax-1; lst_cs=cs;
  if (--cs<0) cs=cmax-1;
  BUF[cs][0]=BUF[lst_cs][0]=nop; cs=lst_cs;
  fputs(prompt,stdout);
  for (key=get_ch();;) {
    if (key>=040 && key<0200 || key=='\t') {
      get_string(BUF[lst_cs],key,&key);
    }
    if (key==k_up || key==k_down) {
      if (key==k_up) { if (++cs==cmax) cs=0; }
      else { if (--cs<0) cs=cmax-1; }
      if (BUF[cs][0]==nop) {
        puts("<wrap> ");
        cs=lst_cs; key=get_ch(); //clearWin(1);
        BUF[lst_cs][0]=0;
      }
      else {
        if (cs!=lst_cs) strcpy(BUF[lst_cs],BUF[cs]); 
        key=0; get_string(BUF[lst_cs],key,&key);
      }
      continue;
    }
    else if (key==k_ret) {
      if (BUF[lst_cs][0]==nop) BUF[lst_cs][0]=0;
      break;
    }
    putchar(07);
    break;  // not reached
  }
  cur_buf=lst_cs;
  return BUF[cur_buf];
}

#ifdef TEST
int main() {
  char *s;
  for (;;) {
    s=get_line();
    printf("\n--> s=[%s]\n",s);
  }
}
#endif
