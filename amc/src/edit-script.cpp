#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Window.h>
#include <TextView.h>
#include <ScrollView.h>
#include "str.h"
#include "widgets.h"
#include "edit-script.h"
#include "amc.h"

const int buf_max=200,
          tv_index_max=200,
          wrap_min=80, // if width is less, no word wrap 
          meas_win=33; // measure window width

extern rgb_color cBlack,
                 cLightYellow,
                 cForeground;

enum {
  eSet, eAdd_Take        // modify script
};

struct Rewr_script {
  const int
    start,
    stop,
    meter;
  bool insert_gap;
  char in_buf[buf_max],
       out_buf[buf_max];
  Rewr_script(int sta,int sto,int m):
      start(sta),stop(sto),meter(m),insert_gap(sto>sta) { }
  int read_time(Str &str,int &pos);
  void rewr_line(bool &ok);
  void rewr_params(Str &str,int &pos,int mode,char *&ob,bool &ok);
};

void cpy_str(char *s,char *&ob) {
  char *ib;
  for (ib=s;*ib;) *(ob++)=*(ib++);
}

void cpy_char(char ch,char *&ob) {
  switch (ch) {
    case ';': cpy_str("; ",ob); break;
    case ' ': cpy_str(" ",ob); break;
    case '#': cpy_str(" #",ob); break;
    case '\n': cpy_str("\n",ob); break;
    default: ob[0]=ch; ob[1]=0; alert("unexpected char (%c)",ch);
  }
}

int Rewr_script::read_time(Str &str,int &pos) {
  str.strtok(in_buf," .,\n;",pos);
  int nr=atoi(str.s) * meter;
  if (str.ch=='.') {
    str.strtok(in_buf," \n;",pos);
    nr+=atoi(str.s);
  }
  return nr;
}

void Rewr_script::rewr_params(Str &str,int &pos,int mode,char *&ob,bool &omit) {
  int tim;
  char *lst_ob,*prev_ob;
  omit=false;
  for (;;) {
    str.strtok(in_buf," :;\n#",pos);
    if (str.ch==':') {
      if (str=="time") {
        cpy_str("time:",ob);
        lst_ob=prev_ob=ob;
        for(;;) {
          tim=read_time(str,pos);
          if (insert_gap) {
            if (tim>=start) tim+=stop-start+1;
          }
          else {
            if (debug) printf("tim=%d start=%d stop=%d\n",tim,start,stop);
            switch (mode) {
              case eAdd_Take:
                if (tim>=start) tim+=stop-start+1;
                else if (tim>=stop) {
                  ob=lst_ob;
                  goto find_comma;
                }
                break;
              case eSet:
                if (tim>start) tim+=stop-start+1;
                else if (tim>=stop) tim=stop;
                break;
            }
          }
          if (tim % meter > 0)
            ob += sprintf(ob,"%d.%d",tim/meter,tim%meter);
          else
            ob += sprintf(ob,"%d",tim/meter);
          find_comma:
          if (str.ch!=',') break;
          if (lst_ob!=ob) {
            lst_ob=ob;
            cpy_str(",",ob);
          }
        }
        if (mode==eAdd_Take && ob==prev_ob) omit=true;
      }
      else {
        cpy_str(str.s,ob);
        cpy_str(":",ob);
        str.strtok(in_buf," ;\n#",pos);
        cpy_str(str.s,ob);
      }
    }
    else
      cpy_str(str.s,ob);
    if (str.ch=='\n' || str.ch=='#') break;
    cpy_char(str.ch,ob);
  }
}

void Rewr_script::rewr_line(bool &ok) {
  int pos=0,
      mode=0;
  char *ob=out_buf,
       *prev_ob;
  bool omit;
  Str str;
  for (;;) {
    prev_ob=ob;
    str.strtok(in_buf," :;\n#",pos);
    if (!str.s[0]);
    else if (str=="put") {
      cpy_str("put ",ob);
      str.strtok(in_buf," ;\n#",pos);
      cpy_str(str.s,ob);
    }
    else if (str=="exit") {
      cpy_str("exit",ob);
    }
    else if (str=="set") {
      cpy_str("set ",ob);
      rewr_params(str,pos,eSet,ob,omit);
    }
    else if (str=="add" ||
             str=="take" ||
             str=="take-nc") {
      cpy_str(str.s,ob);
      cpy_str(" ",ob);
      rewr_params(str,pos,eAdd_Take,ob,omit);
      if (omit) ob=prev_ob;
    }
    else {
      alert("modify script: unknown cmd \"%s\"",str.s);
      ok=false;
      return;
    }

    cpy_char(str.ch,ob);
    if (str.ch=='\n') break;
    if (str.ch=='#') {
      cpy_str(in_buf+pos,ob);
      break;
    }
  }
  *ob=0;
}

void AppWindow::modify_script(BTextView *tv,int start,int end) {
  // supposed: ok = false
  char in_line[buf_max],
       out_line[buf_max];
  Str str;
  if (start==end) {
    alert("modify script: start=%d, end=%d",start,end);
    return;
  }
  bool ok=true;
  Rewr_script rwscr(start,end,appWin->act_meter);
  char *old_text=(char*)malloc(tv->TextLength()+1);
  strcpy(old_text,tv->Text());
  tv->SetText("");
  char *s,*p,*ib;
  for (s=old_text;*s;s=p+1) {
    for (p=s,ib=rwscr.in_buf;;++p) {
      if (!*p) { *(ib++)='\n'; break; }
      else {
        *(ib++)=*p;
        if (*p=='\n') break;
      }
      if (ib-rwscr.in_buf>=buf_max) {
        alert("modify_script: line > %d chars",buf_max);
        return;
      }
    }
    *ib=0;
    rwscr.rewr_line(ok);
    if (debug) printf("inl:[%s] outl:[%s]\n",rwscr.in_buf,rwscr.out_buf);
    if (!ok) break;
    tv->Insert(rwscr.out_buf);
    if (!*p) break; // last line does not end with '\n'
  }
  free(old_text);
}

EditTextView::EditTextView(BRect area,BRect textrect,uint32 resizingMode):
    BTextView(area, 0, textrect, resizingMode, B_WILL_DRAW),
    meas_info(new MeasInfo[tv_index_max]),
    cur_index(-1),
    show_meas(false) {
  SetWordWrap(false);
  SetDoesUndo(true);
}

void EditTextView::clear_meas(BRect rect) {
  rect.left=rect.right-meas_win+2;
  SetHighColor(cForeground);
  FillRect(rect);
  SetHighColor(cBlack);
}

void EditTextView::reset() {
  cur_index=-1;
  if (Bounds().Width()>wrap_min)   // erase measure numbers
    clear_meas(Bounds());
}

void EditTextView::draw_meas(int ind) {
  if (!show_meas) return;
  int y=(int)PointAt(meas_info[ind].pos).y;
  char meas[10];
  int snr=meas_info[ind].snr;
  sprintf(meas," %d.%d",snr/appWin->act_meter,snr%appWin->act_meter);
  SetLowColor(cForeground);
  DrawString(meas,BPoint(Bounds().right-StringWidth(meas),y+11));
  SetLowColor(cLightYellow);
}

void EditTextView::Draw(BRect rect) {
  BTextView::Draw(rect);
  if (rect.Width()>wrap_min) {
    clear_meas(rect);
    for (int ci=0;ci<=cur_index;++ci) draw_meas(ci);
  }
}

bool EditTextView::CanEndLine(int32 offset) { return true; }

void EditTextView::KeyDown(const char* bytes,int32 num) {
  appWin->PostMessage('scrm'); // script modified
  BTextView::KeyDown(bytes,num);
}

EditScript::EditScript(BRect frame) {
  BRect rect(2,2,0,0);
  BRect area=frame;
  area.right-=B_V_SCROLL_BAR_WIDTH;
  BRect textrect=area;
  textrect.OffsetTo(0,0);
  textview=new EditTextView(area,textrect,B_FOLLOW_ALL_SIDES);
  textview->SetViewColor(cLightYellow);
  scrollview=new BScrollView(0, textview, B_FOLLOW_ALL_SIDES, 0, false, true, B_NO_BORDER);
}

void EditScript::read_scriptf(FILE *in) {
  fseek(in,0,SEEK_END);
  off_t fsize=ftell(in);
  char *text=(char*)malloc(fsize);
  fseek(in,0,SEEK_SET);
  fread(text,1,fsize,in);
  textview->SetText(text,fsize);
  free(text);
}

void EditScript::save_scriptf(FILE *out) {
  fwrite(textview->Text(), 1, textview->TextLength(),out);
}

void EditScript::report_meas(int pos,int snr) {
  if (textview->cur_index==tv_index_max-1) return;
  int ind=++textview->cur_index;
  EditTextView::MeasInfo *mi=textview->meas_info+ind;
  mi->snr=snr;
  mi->pos=pos;
  textview->draw_meas(ind);
}

void EditTextView::FrameResized(float width, float height) {
  static bool prev_showm;  // previous show_meas
  BRect textrect = BRect(0,0,width,height);
  if (width<wrap_min) {
    SetWordWrap(false);
    show_meas=false;
    SetTextRect(textrect);
  } else {
    SetWordWrap(true);
    show_meas=true;
    textrect.right-=meas_win;
    SetTextRect(textrect);
    if (prev_showm==false)
      for (int ci=0;ci<=cur_index;++ci) draw_meas(ci);
  }
  prev_showm=show_meas;
}
