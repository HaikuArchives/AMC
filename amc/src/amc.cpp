#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <ScrollView.h>
#include <TextView.h>
#include <Bitmap.h>
#include <Cursor.h>
#include <Mime.h>
#include <NodeInfo.h>
#include <Path.h>

#include "str.h"
#include "widgets.h"
#include "dump_wav.h"
#include "bitmaps.h"
#include "amc.h"
#include "colors.h"
#include "sound.h"
#include "chords.h"
#include "ndist.h"
#include "midi-out.h"
#include "read-wav-data.h"
#include "edit-script.h"
#include "amc-scheme.h"
#include "midi-keyb.h"

const int
  sect_max=40,       // max sections in ScLine
  tunes_max=100,     // max tunes
  max_alerts=5,
  editwin_width=48,  // script edit window
  sect_mus_max=85,   // max sections in music ScLine
  sclin_dist=4,      // line distance
  y_off=14,          // y offset of score lines
  x_off=14,          // x offset
  x_gap=2,           // gap at right side
  sect_len=6,
  m_act_max=9,       // mouse action buttons
  nop=-99,
  eo_arr=-98;        // end of array

const float
  view_vmax=698,     // total hight
  view_hmax=x_off+x_gap+sect_mus_max*sect_len+editwin_width, // music view + script edit window
  scview_vmax=2+y_off+sclin_max*sclin_dist+B_H_SCROLL_BAR_HEIGHT, // height of score view
  maction_width=200, // mouse action buttons
  rbutview_left=247, // distance left side of radio-button views and right side
  buttons_left=74,   // distance left side of buttons and right side
  but_dist=20,
  slider_width=54,   // slider width
  slider_height=42,  // horizontal slider height
  ictrl_height=84,   // instrument control view
  radiob_dist=13,    // radio button distance
  score_info_len=65, // at right side of scores
  checkbox_height=15,// checkbox height
  scview_text=x_off+x_gap+sect_max*sect_len; // text and controls in score view

char *const names[colors_max]={ "black","red","green","blue","brown","purple","orange" };
int colors[colors_max]={ eBlack,eRed,eGreen,eBlue,eBrown,ePurple,eOrange };

enum {
  eScore_start,  // saving and restoring from file
  eScore_end,
  ePlayNote,
  eSetMeter,

  eAdd, eTake, eTake_nc,  // read script
  eFile, eString,

  eText,     // ScInfo tags
  eTempo,
  ePurple_a_harm, ePurple_s_harm, ePurple_attack, ePurple_loc,
  eRed_att_timbre, eRed_timbre, eRed_attack, eRed_decay, eRed_stamp, eRed_loc,
  eGreen_tone, eGreen_attack, eGreen_decay, eGreen_loc,
  eBlack_mod, eBlack_attack, eBlack_decay, eBlack_subband, eBlack_loc,
  eBlue_attack, eBlue_decay, eBlue_piano, eBlue_chorus, eBlue_rich, eBlue_loc,
  eBrown_mod, eBrown_attack, eBrown_decay, eBrown_subband, eBrown_detune, eBrown_loc,
  eOrange_timbre, eOrange_attack, eOrange_decay, eOrange_loc,

  eMusic,                      // score type

  eColorValue, eAmplValue,     // section drawing mode

  eIdle, eTracking, eErasing,  // mouse states
  eMoving, eMoving_hor, eMoving_vert,
  eCopying, eCopying_hor, eCopying_vert, 
  eCollectSel,
  ePortaStart,
  eToBeReset,

  eUp,eDown                    // picture button
};

rgb_color  // exported, so cannot be const
    cWhite	= {255,255,255,255},
    cBlack	= {0,0,0,255},
    cGrey	= {210,210,210,255},
    cGhost	= {180,180,180,150},
    cLightGrey	= {245,245,245,255},
    cDarkGrey	= {100,100,100,255},
    cBackground	={190,210,255,255},
    cPurple	= {210,0,210,255},
    cRed	= {230,0,0,255},
    cBlue	= {0,0,255,255},
    cLightBlue	= {210,255,255,255},
    cDarkBlue   = {0,0,160,255},
    cGreen	= {0,170,0,255},
    cLightGreen	= {0,200,0,255},
    cDarkGreen	= {0,140,0,255},
    cLightYellow = {255,255,200,255},
    cForeground	= {230,230,230,255},
    cBrown	= {190,140,0,255},
    cOrange = {255,140,0,255},
    cPink	= {255,210,255,255};

const rgb_color ampl_color[ampl_max+1] =  // index 1 to 6
              { cPink,
                {240,240,240,255},
                {220,220,220,255},
                {190,190,190,255},
                {160,160,160,255},
                {130,130,130,255},
                {0,0,0,255}
              };


AppWindow *appWin;
BFont *appFont;
const char *wave_out_file="out.wav",
           *midi_out_file="out.mid",
           *tunes_mime="audio/AMC-tunes",
           *script_mime="text/AMC-script",
           *app_mime="application/AMC-app";
bool debug=false;
char textbuf[100];
int alerts;
uint mk_usb_id;   // midi keyboard USB thread
bool dumpwav_okay,
     midiout_okay,
     mk_connected; // midi keyboard connected?
UsbRoster roster;

const bool make_higher[]={ true,false,false,false,true,false,false }, // increase lnr?
           make_lower[]={ false,false,false,true,false,false,true };  // decrease lnr?

void say(char *form,...) {   // debugging
  va_list ap;
  va_start(ap,form);
  printf("say: "); vprintf(form,ap); putchar('\n');
  va_end(ap);
  fflush(stdout);
}

int linenr(float y) {   // y -> line nr
  return float2int((y-y_off)/sclin_dist);
}

int sectnr(float x) {   // x -> section nr
  return float2int((x-x_off-2)/sect_len); // 2 works best
}

int ypos(int lnr) {   // linenr -> y
  return y_off + sclin_dist*lnr;
}

rgb_color color(int col) {
  switch (col) {
    case eBlack:  return cBlack;
    case eGrey:   return cGrey;
    case ePurple: return cPurple;
    case eRed:    return cRed;
    case eOrange: return cOrange;
    case eBlue:   return cBlue;
    case eGreen:  return cGreen;
    case eBrown:  return cBrown;
    case eWhite:  return cWhite;
    case eLightGreen:  return cLightGreen;
    default: alert("color %d?",col); return cBlack;
  }
}

Bitmaps *bitmaps;
BCursor *square_cursor,
        *point_cursor;

struct Scores:Array<Score*,tunes_max> {
  int lst_score;
  Scores();
  Score *new_score(const char*);
  Score *exist_name(const char *nam);
  void swap(int ind1,int ind2);
  void remove(Score*);
} scores;

void alert(const char *form,...) {
  if (alerts>max_alerts) return;
  if (++alerts>max_alerts) sprintf(textbuf,"> %d warnings",max_alerts);
  else {
    va_list ap;
    va_start(ap,form);
    vsprintf(textbuf,form,ap);
    va_end(ap);
  }
  int top=(alerts-1)*20;
  if (!be_app) { puts(textbuf); exit(1); }
  else {
    BPoint pt(10,10);
    if (appWin) pt.Set(appWin->Frame().left-30,appWin->Frame().top);
    call_alert(BRect(pt.x,pt.y+top,pt.x+250,pt.y+top+40),textbuf,&alerts);
  }
}

struct PictButton: Button {
  int mode;
  PictButton(BRect rect, int m, int mes, uint32 resizingMode):
      Button(rect,0,mes,resizingMode),
      mode(m) {
  }
  void drawLabel() {
    float x,y;
    switch (mode) {
      case eUp: 
        x=10; y=10;
        FillTriangle(BPoint(x,y-4),BPoint(x-4,y),BPoint(x+4,y));
        break;
      case eDown:
        x=10; y=12;
        FillTriangle(BPoint(x,y),BPoint(x-4,y-4),BPoint(x+4,y-4));
        break;
    }
  }
};

struct SectData {
  int lnr, snr;
  SectData(int lnr1,int snr1):
    lnr(lnr1),snr(snr1) { }
  bool operator==(SectData &sd) { return lnr==sd.lnr && snr==sd.snr; }
  bool operator<(SectData &sd) { return lnr==sd.lnr ? snr<sd.snr : lnr<sd.lnr; }
  //bool operator>(SectData &sd) { return lnr==sd.lnr ? snr>sd.snr : lnr>sd.lnr; }
};

struct Selected {
  struct ScoreView *sv;
  SLinkedList<SectData> sd_list; 
  bool inv; // list inverted?
  Selected():sv(0),inv(false) { }
  void insert(int lnr,int snr) {
    sd_list.insert(SectData(lnr,snr),!inv);
  }
  void remove(int lnr,int snr) {
    sd_list.remove(SectData(lnr,snr));
  }
  void check_direction(int delta_lnr,int delta_snr) {
    if (!inv && (delta_lnr>0 || delta_snr>0) ||
        inv && (delta_lnr<0 || delta_snr<0)) {
      sd_list.invert();
      inv=!inv;
    }
  }
  void reset() {
    sd_list.reset();
    inv=false;
  }
  void restore_sel();
  StoredNote* store_selection(int& bytes);
} selected;

ScSection::ScSection(int col=eBlack,int amp=0):
  note_col(col),cat(eSilent),sign(0),note_ampl(amp),
  stacc(false),sampled(false),sel(false),
  port_dlnr(0),dlnr_sign(0),port_dsnr(0),nxt_note(0),
  del_start(0),del_end(0) {
}

void ScSection::reset() {  // supposed: sel = false
  static ScSection sec;
  *this=sec;
}

struct ScInfo {
  uint tag;
  ScInfo *next;
  union {
    char *text;
    bool b;
    Array<int8,2>n2;
    Array<int8,harm_max>n5;
    int n;
  };
  ScInfo():tag(0),text(0),next(0) { }
  ScInfo(uint t):tag(t),text(0),next(0) { }
  ~ScInfo() { delete next; }
  void add(ScInfo& info) {
    ScInfo *sci;
    if (!tag) *this=info;
    else {
      for (sci=this;sci->next;sci=sci->next);
      sci->next=new ScInfo(info);
    }
  }
  void reset() {
    delete next;
    tag=0; text=0; next=0;
  }
};

void Score::drawEnd(BView* theV) {
  if (end_sect==nop) return;
  theV->SetHighColor(cBlack);
  theV->SetPenSize(1);
  int x=x_off+end_sect*sect_len;
  theV->StrokeLine(BPoint(x,ypos(17)),BPoint(x,ypos(25)));
}

ScSection* Score::get_section(int lnr,int snr) {
  return lin[lnr]->sect + snr;
}

void Score::time_mark(BView *theV,int snr) {
  int meter=appWin->act_meter;
  if (snr % meter == 0) {
    Str str;
    theV->SetHighColor(cBlack);
    theV->DrawString(str.tos(snr/meter),BPoint(x_off+snr*sect_len,y_off-3));
  }
}

void Score::drawText(BView* theV,int snr) {
  int x=x_off+snr*sect_len,
      y;
  ScInfo* sci;
  theV->SetHighColor(cBlack);
  for (sci=scInfo+snr,y=ypos(sclin_max)+6;sci;sci=sci->next)
    if (sci->tag==eText) { theV->DrawString(sci->text,BPoint(x,y)); y+=10; }
}

struct ScoreViewBase: BView {
  int draw_mode,
      play_start,
      play_stop;
  ScoreViewBase(BRect,uint32,uint32);
  void draw_start_stop(Score *score,int old_snr,int snr,bool begin);
  void enter_start_stop(Score *score,int snr,uint32 mouse_but);
};

void ScSection::drawS_ghost(ScoreViewBase* theV,int snr,int lnr,bool erase) {
  int x=x_off+snr*sect_len,
      y=ypos(lnr);
  BPoint start(x,y),
         end(x+sect_len-1,y);
  if (erase) drawSect(theV,snr,lnr);
  else {
    theV->SetDrawingMode(B_OP_ALPHA);
    theV->SetPenSize(3);
    theV->SetHighColor(cGhost);
    theV->StrokeLine(start,end);
    theV->SetDrawingMode(B_OP_COPY);
  }
}

struct LineCol {
  int col[sclin_max];
  LineCol() {
    int i;
    for (i=0;i<sclin_max;i+=2) col[i]=eWhite;
    for (i=1;i<16;i+=2) col[i]=eGrey;
    for (;i<26;i+=2) col[i]=eBlack;
    for (;i<28;i+=2) col[i]=eGrey;
    for (;i<38;i+=2) col[i]=eLightGreen;
    for (;i<sclin_max;i+=2) col[i]=eGrey;
  }
} linecol;

void ScSection::drawSect(ScoreViewBase* theV,int snr,int lnr) {
  int x=x_off+snr*sect_len,
      y=ypos(lnr);
  BPoint start(x,y),
         end(x+sect_len-1,y);

  theV->SetPenSize(5);
  theV->SetHighColor(cWhite);
  theV->StrokeLine(start,end);
  switch (cat) {
    case eSilent:
      theV->SetPenSize(1);
      --end.x;
      theV->SetHighColor(color(linecol.col[lnr]));
      theV->StrokeLine(start,end);
      if (snr%appWin->act_meter==0 && lnr%2==0) { // timing marks between lines
        theV->SetHighColor(cGrey);
        theV->StrokeLine(BPoint(x,y-1),BPoint(x,y+1));
      }
      break;
    case ePlay:
      drawPlaySect(theV,start,end);
      break;
    default: alert("section cat %d?",cat);
  }
}

ScLine::ScLine(Score* sc):
    len(sc->len),
    sect(new ScSection[len](sc->ncol,sc->nampl)),
    note_sign(0) {
}

void ScSection::drawPortaLine(ScoreViewBase *theV,int snr,int lnr,bool erase) {
  int x=x_off+snr*sect_len,
      y=ypos(lnr);
  BPoint start(x,y),
         end(x+sect_len-1,y);
  theV->SetPenSize(1);
  if (erase) theV->SetHighColor(cWhite);
  else theV->SetHighColor(color(note_col));
  int dlnr= dlnr_sign ? port_dlnr : -port_dlnr;
  theV->StrokeLine(end,BPoint(start.x+(port_dsnr+1)*sect_len,start.y+dlnr*sclin_dist));
}

void drawSignedSect(BView *theV,BPoint& start,BPoint& end,int sign) {
// actually removes upper or lower part from note
  theV->SetPenSize(1);
  theV->SetHighColor(cWhite);
  start.x+=2; 
  if (sign==eHi) { --start.y; --end.y; }
  else if (sign==eLo) { ++start.y; ++end.y; }
  theV->StrokeLine(start,end);
}

void ScSection::drawPlaySect(ScoreViewBase *theV,BPoint &start,BPoint &end) {
  int note_color= nxt_note ? eGrey : note_col;
  pattern patt;
  switch (theV->draw_mode) {
    case eColorValue:
      theV->SetHighColor(color(note_color));
      patt=sel ? B_MIXED_COLORS : B_SOLID_HIGH;
      break;
    case eAmplValue:
      theV->SetHighColor(ampl_color[note_ampl]);
      patt=B_SOLID_HIGH;
      break;
  }
  if (sampled) {
    theV->SetPenSize(1);
    theV->StrokeLine(BPoint(start.x,start.y-1),BPoint(end.x,end.y+1),patt);
    theV->StrokeLine(BPoint(start.x,start.y+1),BPoint(end.x,end.y-1),patt);
  }
  else {
    if (stacc) end.x-=2;
    theV->SetPenSize(3);
    theV->StrokeLine(start,end,patt);
  }
  if (sign) drawSignedSect(theV,start,end,sign);
}

void ScLine::eraseScLine(ScoreViewBase* theV,int lnr,int left=0,int right=0) {
  int nmax= right ? min(len,right) : len,
      x1=x_off+left*sect_len,
      x2=x_off+nmax*sect_len,
      y=ypos(lnr);
  BPoint start(x1,y),
         end(x2,y);

  theV->SetPenSize(5); // erase old line
  theV->SetHighColor(cWhite);
  theV->StrokeLine(start,end);
}

void ScLine::drawScLine(ScoreViewBase* theV,int lnr,int left=0,int right=0) {
  int snr,
      nmax= right ? min(len,right) : len,
      x1=x_off+left*sect_len,
      x2=x_off+nmax*sect_len,
      y=ypos(lnr);
  ScSection *sec,*sec1;
  BPoint start(x1,y),
         end(x2,y);
  rgb_color line_color=color(linecol.col[lnr]);
  pattern patt;

  for (snr=left;snr<nmax;++snr) {
    sec=sect+snr;
    x1=x_off+snr*sect_len;
    x2=x1+sect_len-1;
    start.Set(x1,y);
    end.Set(x2,y);
    switch (sec->cat) {
      case eSilent:
        --end.x;
        theV->SetPenSize(1);
        theV->SetHighColor(line_color);
        theV->StrokeLine(start,end);
        if (snr%appWin->act_meter==0 && lnr%2==0) { // timing marks between lines
          theV->SetHighColor(cGrey);
          theV->StrokeLine(BPoint(x1,y-1),BPoint(x1,y+1));
        }
        break;
      case ePlay:
        sec->drawPlaySect(theV,start,end);
        for (sec1=sec;;sec1=appWin->mn_buf+sec1->nxt_note) {
          if (sec1->port_dlnr)
            sec1->drawPortaLine(theV,snr,lnr,false);
          if (!sec1->nxt_note) break;
        }
        break;
      default: alert("section cat %d?",sec->cat);
    }
  }
}

Score::Score(const char *nam,int length,uint sctype):
    name(const_cast<char*>(nam)),
    ncol(eBlack),
    nampl(4),
    len(length),
    lst_sect(-1),
    end_sect(nop),
    signs_mode(0),
    is_music(sctype==eMusic),
    scInfo(sctype==eMusic ? new ScInfo[len]() : 0),
    rbut(0) {
  int i;
  for (i=0;i<sclin_max;i++) lin[i]=new ScLine(this);
}

int Score::to_half(int lnr,int sign) {
  int ind=lnr%7;
                //  b a g f e d c
  static int ar[]={ 0,2,4,6,7,9,11 };
  return ar[ind] + (sign==eHi ? -1 : sign==eLo ? 1 : 0) + (lnr-ind)/7*12;
}

void Score::from_half(int n,int& lnr,int& sign) {
  int ind=n%12,
      *ar, *s;
                   // b   bes a   as  g   fis f   e   dis d   cis c
  static int ar_0[]={ 0,  0,  1,  1,  2,  3,  3,  4,  5,  5,  6,  6 },
             s_0[]= { 0,  eLo,0,  eLo,0,  eHi,0,  0,  eHi,0,  eHi,0 },

                   // b   bes a   as  g   ges f   e   es d   des c
             ar_f[]={ 0,  0,  1,  1,  2,  2,  3,  4,  4,  5,  5,  6 },
             s_f[]= { 0,  eLo,0,  eLo,0,  eLo,0,  0,  eLo,0,  eLo,0 },

                   // b   ais a   gis  g   fis f   e   dis d   cis c
             ar_s[]={ 0,  1,  1,  2,  2,  3,  3,  4,  5,  5,  6,  6 },
             s_s[]= { 0,  eHi,0,  eHi,0,  eHi,0,  0,  eHi,0,  eHi,0 };
  switch (signs_mode) {
    case eFlat: ar=ar_f; s=s_f; break;
    case eSharp: ar=ar_s; s=s_s; break;
    case 0: ar=ar_0; s=s_0; break;
    default: ar=ar_0; s=s_0; alert("signs_mode?");
  }
  lnr=ar[ind]+(n-ind)/12*7;
  sign=s[ind];
}

bool Score::check_len(int required) {
  ScLine *old_lin;
  ScInfo *old_info;
  int n,n2,
      old_len=len;
  if (len>=required) return false;
  if (debug) printf("len incr: len=%d req=%d\n",len,required);
  while (len<required) len+=old_len;
  for (n=0;n<sclin_max;n++) {
    old_lin=lin[n];
    lin[n]=new ScLine(this);
    lin[n]->note_sign=old_lin->note_sign;
    for (n2=0;n2<old_len;n2++)
      lin[n]->sect[n2]=old_lin->sect[n2];
    delete[] old_lin;
  }
  if (scInfo) {
    old_info=scInfo;
    scInfo=new ScInfo[len]();
    for (n=0;n<old_len;n++) {
      scInfo[n]=old_info[n];
      old_info[n].next=0; // to prohibit deletion of *next sections
    }
    delete[] old_info;
  }
  return true;
}

bool eq(ScSection* one,ScSection* two) {  // NOT considered: next
  return one->cat==two->cat &&
         one->sign==two->sign &&
         one->note_col==two->note_col;
}

void Score::copy(Score *from) { // name NOT copied
                                // end_sect, amplitude and color copied
  int snr,lnr,
      stop;
  end_sect=from->end_sect;
  if (end_sect>nop) { check_len(end_sect+1); stop=end_sect; }
  else { stop=from->len; check_len(stop); }
  for (lnr=0;lnr<sclin_max;lnr++) {
    lin[lnr]->note_sign=from->lin[lnr]->note_sign;
    for (snr=0;snr<stop;++snr) {
      ScSection& to=lin[lnr]->sect[snr];
      to=from->lin[lnr]->sect[snr];
      to.sel=0;
    }
  }
  ncol=from->ncol;
  nampl=from->nampl;
  signs_mode=from->signs_mode;
}

void Score::copy_keyb_tune(KeybTune &kt) {
  int n,n2;
  KeybNote *note;
  ScSection *to;
  note=kt.buf + kt.cur_ind;
  end_sect=note->snr+note->dur+1;
  check_len(end_sect);
  for (n=0;n<=kt.cur_ind;++n) {
    note=kt.buf+n;
    to=lin[note->lnr]->sect + note->snr;
    for (n2=0;n2<note->dur;++n2,++to) {
      to->cat=ePlay;
      to->sign=note->sign;
    }
  }
}

void Score::add_copy(Score *sc,
    int start,int stop,int tim,int ampl,int shift,int raise,ScoreViewBase* theV) {
    // name, ncol, nampl NOT copied
  int n,
      snr,snr1,
      lnr,lnr1,
      sign,
      note_num=0;
  ScSection *from,
            *to,*to1;
  check_len(tim+stop-start);
  if (is_music) {
    ScInfo info(eText);
    info.text=sc->name;
    scInfo[tim].add(info);
    time_mark(theV,tim);
    drawText(theV,tim);
    drawEnd(theV);
  }
  for (snr=start,snr1=tim-1;snr<stop;++snr) {
    ++snr1;
    for (lnr=0;lnr<sclin_max;lnr++) {
      from=sc->get_section(lnr,snr);
      if (from->cat==ePlay && (!appWin->solo->value || from->note_col==appWin->act_color)) {
        if (snr==start || (from-1)->cat!=ePlay || !eq(from-1,from)) // find start of note
          ++note_num;
        if (raise || shift) {
          if (raise) {
            lnr1=lnr-raise;
            sign=sc->lin[lnr1]->note_sign; // sign of to-linenr of from-score
          }
          if (shift) from_half(to_half(lnr,from->sign)-shift,lnr1,sign);
        }
        else { lnr1=lnr; sign=from->sign; }
        if (lnr1>=sclin_max || lnr1<0) continue;
        to1=to=get_section(lnr1,snr1);
        if (to->cat==ePlay)  // multiple note?
          for (;to1->note_col!=from->note_col || to1->sampled!=from->sampled; // equal colors cannot be played together
                to1=appWin->mn_buf+to1->nxt_note) {
            if (!to1->nxt_note) {
              if (appWin->mn_end==mn_max-1) { alert("> %d multi notes",mn_max); break; }
              to1->nxt_note=++appWin->mn_end;
              to1=appWin->mn_buf+appWin->mn_end;
              break;
            }
          }
        *to1=*from;
        to1->sel=false;
        to1->sign=sign;
        if (ampl>nop) to1->note_ampl=ampl;
        if (is_music) {
          to->drawSect(theV,snr1,lnr1);
          if (to1->port_dlnr) to1->drawPortaLine(theV,snr1,lnr1,false);
        }
      }
    }
  }
  if (snr1>lst_sect) {
    end_sect=snr1+1;
    check_len(end_sect+1);
    if (is_music) {
      if (lst_sect>=0)
        for (n=0;n<sclin_max;++n)  // erase old end-line
          lin[n]->sect[lst_sect+1].drawSect(theV,lst_sect+1,n);
      time_mark(theV,end_sect);
      drawText(theV,end_sect);
      drawEnd(theV);
    }
    lst_sect=snr1;
  }
}

void Score::reset() {
  ncol=eBlack;
  nampl=4;
  lst_sect=-1;
  end_sect=nop;
  signs_mode=0;
  int lnr,snr;
  if (is_music) {
    for (lnr=0;lnr<sclin_max;lnr++)
      for (snr=0;snr<len;snr++) lin[lnr]->sect[snr].reset();
    for (snr=0;snr<len;snr++) scInfo[snr].reset();
  }
  else if (len>sect_max) {
    len=sect_max;
    for (lnr=0;lnr<sclin_max;lnr++) {
      delete[] lin[lnr]->sect;
      lin[lnr]->note_sign=0;
      lin[lnr]->sect=new ScSection[len](ncol,nampl);
    }
  }
  else {
    for (lnr=0;lnr<sclin_max;lnr++) {
      ScLine *line=lin[lnr];
      line->note_sign=0;
      for (snr=0;snr<len;snr++) line->sect[snr].reset();
    }
  }
}

Scores::Scores():lst_score(-1) {
  for (int i=0;i<tunes_max;i++) buf[i]=0;
}

StdView::StdView(BRect rect):BView(rect,0,B_FOLLOW_RIGHT,B_WILL_DRAW) {
  SetFont(appFont);
  SetViewColor(cForeground);
  SetLowColor(cForeground);
}

void ScoreViewBase::draw_start_stop(Score *score,int old_snr,int snr,bool begin) {
  int x,
      y=7;
  if (old_snr>0) {
    x=x_off+old_snr*sect_len;
    Invalidate(BRect(x,y-5,x+sect_len,y+5));
  }
  if (snr>0) {
    x=x_off+snr*sect_len;
    SetHighColor(cRed);
    if (begin) FillTriangle(BPoint(x,y+5),BPoint(x+sect_len,y),BPoint(x,y-5));
    else FillTriangle(BPoint(x+sect_len,y+5),BPoint(x,y),BPoint(x+sect_len,y-5));
  }
}

ScoreViewBase::ScoreViewBase(BRect rect,uint32 rmode,uint32 flags):
    BView(rect,0,rmode,flags),
    draw_mode(eColorValue),
    play_start(0),play_stop(-1) {
}

struct ScoreViewStruct {
  int id;
  struct ScoreSView *scroll_view;
  struct ScoreView *scv_view;
  struct ScoreViewText *scv_text;
  struct ZoomWindow *zoomWin;
  ScoreViewStruct(BRect rect,int ident);
  void assign_score(Score*);
  void reset();
  void invalidate();
};

struct ScoreView: ScoreViewBase {
  ScoreViewStruct *theViewStruct;
  Score *score;
  ScLine *cur_line;
  ScSection *cur_sect;   // set when mouse down
  int state,
      cur_lnr, cur_snr,  // set when mouse down
      prev_snr,          // previous visited section nr
      delta_lnr, delta_snr;
  BPoint cur_point,      // set when mouse down
         prev_point;     // set when mouse moved
  ScoreView(BRect rect,ScoreViewStruct*);
  void Draw(BRect rect);
  void redraw(bool meter);
  void upd_endline(int snr);
  void MouseDown(BPoint);
  void MouseMoved(BPoint, uint32 location, const BMessage *);
  void MouseUp(BPoint);
  void drawSigns();
  void select_all(bool all);
  void modify_sel(int mes);
  void select_column(int snr);
};

struct ScScrollBar: BScrollBar {
  ScScrollBar(BRect frame,BView *target,float mini,float maxi):
      BScrollBar(frame,"",target,mini,maxi,B_HORIZONTAL) {
    SetResizingMode(B_FOLLOW_LEFT_RIGHT);
  }
};

struct ScoreSView {
  ScoreView *scoreView;
  BScrollBar *scrollbar;
  ScoreSView(BRect rect,ScoreViewStruct *svs) {
    scoreView=new ScoreView(BRect(rect.left,rect.top,rect.right,rect.bottom-B_H_SCROLL_BAR_HEIGHT),svs);
    scrollbar=new ScScrollBar(BRect(rect.left,rect.bottom-B_H_SCROLL_BAR_HEIGHT,rect.right,rect.bottom),
                             scoreView,0,scview_text);
    scrollbar->SetRange(0,0);
  }
};

struct ZoomView: BView {
  ScoreViewStruct *theVS;
  int &left,&right;
  ZoomView(BRect rect,ScoreViewStruct *);
  void Draw(BRect);
  void MouseDown(BPoint);
  void sectnr(float x,int &snr,int &pos) {
    float x_f=x - x_gap- subdiv*2;  // 2 works okay
    snr=float2int(x_f/(sect_len*subdiv))+left;
    pos=float2int(x_f/sect_len) % subdiv;
  }
};

void ScSection::drawZoomSilentSect(ZoomView *theV,rgb_color col,
                                   BPoint &start,BPoint &end,ScSection *prev=0) {
  theV->SetPenSize(1);
  if (prev && prev->del_end) start.x += prev->del_end*sect_len;
  --end.x;
  theV->SetHighColor(col);
  theV->StrokeLine(start,end);
}

void ScSection::drawZoomPlaySect(ZoomView *theV,BPoint &start,BPoint &end,
                                 ScSection *prev=0,ScSection *next=0) {
  int n;
  theV->SetPenSize(3);
  if (sampled) {
    theV->SetHighColor(cPink);  // sampled note
    theV->StrokeLine(start,end);
    return;
  }
  BPoint pt3(end);
  pt3.x+=(subdiv-1)*sect_len;
  theV->SetHighColor(cWhite);  // erase next section
  theV->StrokeLine(end,pt3);
  if (next) {
    BPoint pt1(end),pt2(end);
    pt1.x+=next->del_start*sect_len;
    pt2.x+=subdiv*sect_len;
    theV->SetHighColor(color(next->note_col)); // draw next section
    theV->StrokeLine(pt1,pt2);
    if (next->sign) {
      drawSignedSect(theV,pt1,pt2,next->sign); theV->SetPenSize(3);
    }
  }
  if (del_end) end.x+=del_end*sect_len;
  if (stacc) end.x-=2;
  if (prev && prev->del_end) {
    BPoint pt1(start);
    pt1.x+=prev->del_start*sect_len;
    theV->SetHighColor(color(prev->note_col));
    theV->StrokeLine(start,pt1);
    start.x+=del_start*sect_len;
    if (del_start > prev->del_end) {
      theV->SetHighColor(cWhite);
      theV->StrokeLine(pt1,start);
    }
    theV->SetHighColor(color(note_col));
    theV->StrokeLine(start,end);
  }
  else {
    if (del_start) {
      BPoint pt1(start);
      start.x+=del_start*sect_len;
      theV->SetHighColor(cWhite);
      theV->StrokeLine(pt1,start);
    }
    theV->SetHighColor(color(note_col));
    theV->StrokeLine(start,end);
  }
  if (sign) drawSignedSect(theV,start,end,sign);
}

void endpoints(int snr,int offset,BPoint &start,BPoint &end,float y) { // for zoomed view
  int x1=x_gap + (snr-offset) * sect_len * subdiv,
      x2=x1 + sect_len*subdiv - 1;
  start.Set(x1,y);
  end.Set(x2,y);
}

void ScLine::drawZoomScLine(ZoomView* theV,int lnr,int left,int right) {
  int snr,
      y=ypos(lnr),
      line_color=linecol.col[lnr];
  ScSection *sec,*prv_sec;
  BPoint start,end;
  for (snr=left;snr<right;++snr) {
    sec=sect+snr;
    endpoints(snr,left,start,end,y);
    switch (sec->cat) {
      case eSilent:
        if (line_color==eWhite) break;
        prv_sec= snr>0 && (sec-1)->cat==ePlay ? sec-1 : 0;
        sec->drawZoomSilentSect(theV,snr%2 ? cRed : cGreen,start,end,prv_sec);
        break;
      case ePlay:
        sec->drawZoomPlaySect(theV,start,end);
        break;
    }
  }
}

struct ZoomWindow: BWindow {
  ZoomView *view;
  ZoomWindow(BPoint top,ScoreViewStruct *);
  void MessageReceived(BMessage*);
};

ZoomView::ZoomView(BRect rect,ScoreViewStruct *svs):
    BView(rect,0,B_FOLLOW_LEFT_RIGHT,B_WILL_DRAW),
    theVS(svs),
    left(svs->scv_view->play_start),
    right(svs->scv_view->play_stop) {
  Button *but=new Button(BRect(2,Bounds().bottom-20,0,0),"close",'zomC');
  but->theMessage->AddPointer("",theVS);
  AddChild(but);
}

void ZoomWindow::MessageReceived(BMessage *mes) {
  switch (mes->what) {
    case 'zomC': // close window
      appWin->PostMessage(mes);
      break;
    case 'zInv': // redraw
      view->Invalidate();
      break;
    default:
      BWindow::MessageReceived(mes);
   }
}

ZoomWindow::ZoomWindow(BPoint top,ScoreViewStruct *svs):
    BWindow(BRect(top.x,top.y,top.x+250,top.y+2*y_off+sclin_max*sclin_dist+20),
            0,B_MODAL_WINDOW_LOOK,B_NORMAL_WINDOW_FEEL,
            B_OUTLINE_RESIZE|B_ASYNCHRONOUS_CONTROLS),
    view(new ZoomView(Bounds(),svs)) {
  AddChild(view);
}

void ScoreView::drawSigns() {
  int lnr,
      y,
      sign;
  SetDrawingMode(B_OP_OVER);
  for (lnr=0;lnr<sclin_max;++lnr) {
    y=ypos(lnr)-3;
    sign=score->lin[lnr]->note_sign;
    if (sign)
      DrawBitmap(bitmaps->get(sign==eHi ? eSharp : eFlat),BPoint((lnr%7 & 1)==0 ? 2 : 7,y));
  }
  SetDrawingMode(B_OP_COPY);
  for (lnr=0;lnr<=sclin_max;++lnr) {  // 1 extra
    SetHighColor(lnr%7==4 || lnr%7==0 ? cBlue : cLightBlue); // small lines at left side
    y=ypos(lnr)-2;
    StrokeLine(BPoint(0,y),BPoint(1,y));
  }
}

void Score::put_chord(ScoreView *sv) {
  int n,
      lnr,snr,
      sign,
      dist=appWin->chordsWin->the_distance;
  Array<int,10> &chord_notes=appWin->chordsWin->chord_notes;
  ScSection *sect;
  for (n=0;chord_notes[n]>nop;++n) {
    // offset in chord_notes[]: note B = 0, note C = 6, middle C = 6 + 3 * 7 = 27 , is equal to 47 semi-tones
    if (n==0) from_half(47-dist,lnr,sign);
    else from_half(47-chord_notes[n]-dist,lnr,sign);
    if (end_sect>nop) {
      snr=end_sect+1;
      check_len(snr+1);
    }
    else
      snr=0;
    selected.insert(lnr,snr);
    sect=get_section(lnr,snr);
    sect->reset();
    sect->cat=ePlay;
    sect->sign=sign;
    sect->note_ampl=sv->score->nampl;
    sect->note_col=sv->score->ncol;
    sect->sel=true;
    sect->drawSect(sv,snr,lnr);
  }
}
  
void Score::set_signs(int flatn,int sharpn) {
  int n,lnr;
  for (lnr=6;lnr>=0;--lnr) {
    if (flatn & 1) for (n=lnr;n<sclin_max;n+=7) lin[n]->note_sign=eLo;
    else if (sharpn & 1) for (n=lnr;n<sclin_max;n+=7) lin[n]->note_sign=eHi;
    flatn>>=1; sharpn>>=1;
  }
}

void Score::tone2signs() {   // C = 0, B = 12
  const int f=eLo,s=eHi;
  static int signs[][7]= {
    //  B A G F E D C
      { 0,0,0,0,0,0,0 },  // C
      { f,f,f,0,f,f,0 },  // Des
      { 0,0,0,s,0,0,s },  // D
      { f,f,0,0,f,0,0 },  // Es
      { 0,0,s,s,0,s,s },  // E
      { f,0,0,0,0,0,0 },  // F
      { 0,s,s,s,s,s,s },  // Fis
      { f,f,f,f,f,f,0 },  // Ges ( = Fis)
      { 0,0,0,s,0,0,0 },  // G
      { f,f,0,0,f,f,0 },  // As
      { 0,0,s,s,0,0,s },  // A
      { f,0,0,0,f,0,0 },  // Bes
      { 0,s,s,s,0,s,s }   // B
    },          
             // C Des   D      Es    E      F     Fis    Ges   G      As    A      Bes   B
    smode[] = { 0,eFlat,eSharp,eFlat,eSharp,eFlat,eSharp,eFlat,eSharp,eFlat,eSharp,eFlat,eSharp };
  int n,lnr,
      key_nr=appWin->chordsWin->the_key_nr;
  for (lnr=6;lnr>=0;--lnr) {
    for (n=lnr;n<sclin_max;n+=7) lin[n]->note_sign=signs[key_nr][lnr];
  }
  signs_mode=smode[key_nr];
}

struct RotDisplay: public RotateChoice {
  void draw(int nr) {
    SetHighColor(cBlack);
    MovePenTo(BPoint(3,12));
    switch (nr) {
      case 0: DrawString("instr"); break;
      case 1: DrawString("ampl"); break;
    }
  }
  RotDisplay(BRect rect,int msg):
      RotateChoice(rect,msg,2) {
    SetFont(appFont);
    SetViewColor(cLightBlue);
    SetLowColor(cLightBlue);
  }
};

struct ScoreViewText: StdView {
  ScoreViewStruct *theViewStruct;
  UpdText *sc_name;
  HSlider *sc_ampl;
  UpDown *ud;
  CheckBox *zoom,
           *set_repeat;
  RotDisplay *display_mode;
  ScoreViewText(BRect rect,ScoreViewStruct* v):
      StdView(rect),
      theViewStruct(v),
      sc_name(new UpdText(this,BPoint(2,12))),
      sc_ampl(new HSlider(BRect(2,18,2+slider_width,0),"amplitude",'ampl',1,ampl_max)) {
    sc_ampl->SetLimitLabels("1","6");
    sc_ampl->SetValue(4);
    sc_ampl->theMessage->AddPointer("",theViewStruct);
    AddChild(sc_ampl);

    appWin->active_scoreCtrl->AddButton(this,BPoint(2,68),"active",theViewStruct->id);

    zoom=new CheckBox(BPoint(2,92),"zoom",'zom ');
    zoom->theMessage->AddPointer("",theViewStruct);
    AddChild(zoom);

    set_repeat=new CheckBox(BPoint(2,108),"repeat",'rept',B_FOLLOW_RIGHT);
    set_repeat->theMessage->AddPointer("",theViewStruct);
    AddChild(set_repeat);

    display_mode=new RotDisplay(BRect(14,145,44,160),'dmod');
    display_mode->theMessage->AddPointer("",theViewStruct);
    AddChild(display_mode);

    Button *but=new Button(BRect(2,170,0,0),"play",'play');
    but->theMessage->AddPointer("",theViewStruct);
    AddChild(but);
  }
  void Draw(BRect) {
    SetHighColor(cBlack);
    DrawString("display",BPoint(2,display_mode->Frame().top-4));
  }
};

void ScoreViewStruct::assign_score(Score *sc) {
  reset();
  scv_view->score=sc;
  scv_text->sc_ampl->SetValue(sc->nampl);
  scv_view->play_start=0; scv_view->play_stop=-1;
  scroll_view->scrollbar->SetRange(0,(scv_view->score->len-sect_max)*sect_len);
}

void ScoreViewStruct::invalidate() {
  if (scv_view->score)
    scroll_view->scrollbar->SetRange(0,(scv_view->score->len-sect_max)*sect_len);
  else
    scroll_view->scrollbar->SetRange(0,0);
  scroll_view->scrollbar->SetValue(0);
  scv_view->Invalidate();
  scv_text->Invalidate();
}

ScoreView::ScoreView(BRect rect,ScoreViewStruct *svs):
    ScoreViewBase(rect,B_FOLLOW_LEFT_RIGHT,B_WILL_DRAW),
    score(0),
    cur_line(0),cur_sect(0),
    theViewStruct(svs),
    state(eIdle) {
  SetFont(appFont);
  SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY); // for drawing ghost sections
}

void ZoomView::Draw(BRect) {
  ScoreView *sv=theVS->scv_view;
  if (sv->score)
    for (int n=0;n<sclin_max;++n) 
      sv->score->lin[n]->drawZoomScLine(this,n,left,right);
}

void ScoreView::Draw(BRect) {
  SetHighColor(cBlack);
  if (score) {
    int n;
    theViewStruct->scv_text->sc_name->textColor=color(score->ncol);
    theViewStruct->scv_text->sc_name->write(score->name);
    for (n=0;n<sclin_max;++n)
      score->lin[n]->eraseScLine(this,n);
    for (n=0;n<sclin_max;++n)
      score->lin[n]->drawScLine(this,n);
    drawSigns();
    for (n=0;n<max(sect_max,score->end_sect+1);++n) score->time_mark(this,n);
    draw_start_stop(score,0,play_start,true);
    draw_start_stop(score,0,play_stop,false);
    score->drawEnd(this);
  }
}

void ScoreView::redraw(bool clear_meter) {  // less disturbance then regular Invalidate
  int n;
  BRect rect(Bounds());
  SetHighColor(cWhite);
  FillRect(BRect(0,0,x_off,rect.bottom)); // clear signs
  if (clear_meter)
    FillRect(BRect(0,0,rect.right,y_off)); // clear meter numbers
  Draw(Bounds());
}

void ScoreView::upd_endline(int snr) {
  int n,
      old_len;
  ScSection* sect;
  if (score->end_sect>nop || score->end_sect==snr)   // erase old end
    for (n=0;n<sclin_max;++n) {
      sect=score->lin[n]->sect+score->end_sect;
      sect->drawSect(this,score->end_sect,n);
    }
  score->end_sect=snr;
  old_len=score->len;
  if (score->check_len(snr+1)) {
    for (n=0;n<sclin_max;++n)
      score->lin[n]->drawScLine(this,n,old_len,score->len);
    theViewStruct->scroll_view->scrollbar->SetRange(0,(score->len-sect_max)*sect_len);
  }
  for (n=0;n<=snr;++n) score->time_mark(this,n);
  score->drawEnd(this);
}

struct MusicView: ScoreViewBase {
  struct MusicSView* theSView;
  Score *score;
  int musv_len;
  MusicView(BRect rect,MusicSView* sv):
      ScoreViewBase(rect,B_FOLLOW_TOP_BOTTOM|B_FOLLOW_LEFT,B_WILL_DRAW),
      theSView(sv),
      score(new Score("music",sect_mus_max,eMusic)),
      musv_len(sect_mus_max) {
    SetFont(appFont);
  }
  void Draw(BRect rect);
  void redraw(bool);
  void reset(bool reset_start) {
    score->reset();
    if (reset_start) { play_start=0; play_stop=-1; }
    appWin->mn_end=0;
    for (int n=0;n<mn_max;++n) appWin->mn_buf[n].reset();
  }
  void upd_info(int t,ScInfo& sci) {
    if (t) score->check_len(t+1);
    score->scInfo[t].add(sci);
  }
  void MouseDown(BPoint);
};

struct Question: StdView {
  struct QuBHandler:BHandler {
    int mode;
    QuBHandler():BHandler() { }
    void MessageReceived(BMessage *mes) { mode=mes->what; }
  } handler;
  TextControl *textControl;
  UpdTextBg *text;
  Question(BRect rect): StdView(rect),
      textControl(new TextControl(BRect(26,16,rect.Width()-2,0),'ctrl',B_FOLLOW_LEFT)),
      text(new UpdTextBg(this,BPoint(2,10))) {
    AddChild(textControl);
    AddChild(new Button(BRect(2,16,22,0),"ok",'ctrl'));
  }
  void Draw(BRect) {
    text->write(0);
  }
};

struct MeterView: StdView {
  RButtonArrowCtrl *ctrl;
  RadioButton *rbut6,*rbut8,*rbut12;
  BPoint top;
  MeterView(BRect rect):
      StdView(rect),
      top(BPoint(2,12)),
      ctrl(new RButtonArrowCtrl('setm')) {
    rbut6=ctrl->AddButton(this,top,"6",6); top.y+=radiob_dist;
    rbut8=ctrl->AddButton(this,top,"8",8); top.y+=radiob_dist;
    rbut12=ctrl->AddButton(this,top,"12",12);
    redraw();
  }
  void Draw(BRect rect) {
    DrawString("meter",BPoint(2,10));
  }
  void redraw() {
    switch (appWin->act_meter) {
      case 6: rbut6->SetValue(true); break;
      case 8: rbut8->SetValue(true); break;
      case 12: rbut12->SetValue(true); break;
    }
  }
};

struct TunesView: StdView {
  BPoint top;
  RButtonArrowCtrl *ctrl;
  TunesView(BRect rect):
      StdView(rect),
      top(BPoint(2,10)),
      ctrl(new RButtonArrowCtrl('tune'))  {
    Score* sc;
    for (int n=0;n<=scores.lst_score;++n) {
      sc=scores[n];
      sc->rbut=ctrl->AddButton(this,top,sc->name,n);
      top.y+=radiob_dist;
    }
  }
  void reset(int lst_score) {
    Score *sc;
    for (int n=0;n<=lst_score;++n) {
      sc=scores[n];   // score not deleted
      RemoveChild(sc->rbut);
      delete(sc->rbut);
      sc->rbut=0;
    }
    top.Set(2,10);
  }
  void Draw(BRect) { DrawString("tunes",BPoint(2,10)); }
};

struct TunesSView: StdView {
  TunesView *tunesView;
  BScrollView *scrollView;
  TunesSView(BRect rect): StdView(rect) {
    rect.right-=B_V_SCROLL_BAR_WIDTH;
    tunesView=new TunesView(rect);
    scrollView=new BScrollView(0,tunesView,B_FOLLOW_RIGHT,0,false,true,B_NO_BORDER);
    scrollView->ScrollBar(B_VERTICAL)->SetRange(0,(2+tunes_max)*radiob_dist-rect.Height());
  }
};

struct TempoView: StdView {
  HSlider *tempo;
  void show_val() {
    tempo->SetText("%d",10 * tempo->value);
    tempo->UpdateText();
  }
  void redraw_sliders() {
    if (appWin->act_tempo < 6 || appWin->act_tempo > 16) appWin->act_tempo=11;
    tempo->SetValue(appWin->act_tempo);
    show_val();
  }
  TempoView(BRect rect): StdView(rect) {
    tempo=new HSlider(BRect(0,0,slider_width,0),"tempo",0,6,16);
    tempo->SetValue(11);
    tempo->modMessage=new BMessage('tmpo');
    tempo->SetText("110");
    AddChild(tempo);
  }
};

struct MouseAction: BView {
  RButtonAM1KnobCtrl *ctrl;
  BPoint top;
  Array<RadioButton*,m_act_max>buttons;
  int *but_mess;
  MouseAction(BPoint t):
      BView(BRect(t.x,t.y,t.x+maction_width,t.y+2*but_dist),0,B_FOLLOW_LEFT,B_WILL_DRAW),
      ctrl(new RButtonAM1KnobCtrl('msta')),
      top(BPoint(0,0)) {
    SetFont(appFont);
    SetViewColor(cBackground);
    SetLowColor(cBackground);
    ctrl->min_width=20;
    static char
      *label_r[m_act_max]={      // radio buttons
        "select","sel all","sel color","move","copy","portando","sharp","flat","normal"
      },
      *label[5]={ "unselect","re-color","amp -","amp+","delete"};   // normal buttons
    static int
      mess_r[m_act_max]={ 'sel','all','scol','move','copy','prta','up','do','ud'},
      mess[5]={ 'uns','rcol','amp-','amp+','del ' };
    but_mess=mess_r;
    int n;
    for (n=0;n<6;++n) {
      buttons[n]=ctrl->AddButton(this,top,label_r[n],mess_r[n]);
      buttons[n]->textColor=cRed;
      top.x += buttons[n]->dx()+2;
    }
    for (n=0;n<1;++n) {
      Button *but=new Button(BRect(top.x,top.y,top.x+25,0),label[n],mess[n]);
      but->textColor=cDarkBlue;
      AddChild(but);
      top.x += but->dx()+2;
    }
    ResizeTo(top.x,Bounds().bottom);
    top.x=0; top.y=but_dist;
    for (n=6;n<m_act_max;++n) {
      buttons[n]=ctrl->AddButton(this,top,label_r[n],mess_r[n]);
      top.x += buttons[n]->dx()+2;
    }
    top.x+=10;
    for (n=1;n<5;++n) {
      Button *but=new Button(BRect(top.x,top.y,top.x+25,0),label[n],mess[n]);
      but->textColor=cDarkBlue;
      AddChild(but);
      top.x += but->dx()+2;
    }
  }
  void set_active(int ch) {
    for (int n=0;n<m_act_max;++n)
      if (but_mess[n]==ch) { 
        buttons[n]->SetValue(true);
        break;
      }
  }
  void reset() {
    ctrl->reset();
    appWin->act_action=nop;
  }
};

struct RotStereo: public RotateChoice {
  void draw(int);
  RotStereo(BRect rect,int msg):RotateChoice(rect,msg,3) {
    SetViewColor(cLightBlue);
  }
};

void RotStereo::draw(int nr) {
  SetHighColor(cBlack);
  switch (nr) {
    case 0: {
        static BPoint p1[] = { BPoint(7,2),BPoint(2,5),BPoint(7,8) };
        static BPoint p2[] = { BPoint(11,2),BPoint(6,5),BPoint(11,8) };
        StrokePolygon(p1,3,false);
        StrokePolygon(p2,3,false);
      }
      break;
    case 1:
      StrokeLine(BPoint(7,2),BPoint(7,8));
      break;
    case 2: {
        static BPoint p1[] = { BPoint(3,2),BPoint(8,5),BPoint(3,8) };
        static BPoint p2[] = { BPoint(7,2),BPoint(12,5),BPoint(7,8) };
        StrokePolygon(p1,3,false);
        StrokePolygon(p2,3,false);
      }
      break;
  }
}

struct ColorView: StdView {
  RButtonArrowCtrl *ctrl;
  BPoint top;
  RotStereo *stereo[colors_max];
  ColorView(BRect rect):
      StdView(rect),
      ctrl(new RButtonArrowCtrl('colr')) {
    RadioButton *rbut;
    BRect rect_r(0,0,14,10);
                               // black red green blue brown purple orange
    static int place[colors_max]={  0,   2,   5,   4,   1,     6,     3 },
               loc[colors_max]=  {  2,   2,   0,   1,   0,     2,     2 };  
    for (int n=0;n<colors_max;n++) {
      top.Set(2,12+place[n]*radiob_dist);
      rect_r.OffsetTo(rect.Width()-15,15+place[n]*radiob_dist);
      rbut=ctrl->AddButton(this,top,names[n],colors[n]);
      rbut->textColor=color(colors[n]);
      if (n==0) rbut->SetValue(true);

      stereo[n]=new RotStereo(rect_r,0);
      stereo[n]->value=loc[n];
      AddChild(stereo[n]);
      connect_stereo(colors[n],&stereo[n]->value);
    }
  }
  void redraw_sliders() {
    for (int n=0;n<colors_max;++n) stereo[n]->Invalidate();
  }
  void Draw(BRect) {
    DrawString("colors",BPoint(2,10));
  }
};

struct MScrollBar: BScrollBar {
  MScrollBar(BRect frame,BView *target,float mini,float maxi):
      BScrollBar(frame,"",target,mini,maxi,B_HORIZONTAL) {
    SetResizingMode(B_FOLLOW_BOTTOM);
  }
};

struct MusicSView {
  MusicView *musicView;
  BScrollBar* scrollbar;
  MusicSView(BRect rect) {
    musicView=new MusicView(BRect(rect.left,rect.top,rect.right,rect.bottom-B_H_SCROLL_BAR_HEIGHT),this);
    scrollbar=new MScrollBar(BRect(rect.left,rect.bottom-B_H_SCROLL_BAR_HEIGHT,rect.right,rect.bottom),
                             musicView,0,view_hmax);
  }
};

void MusicView::Draw(BRect rect) {
  int n,
      left=static_cast<int>((Bounds().left-x_off)/sect_len);
  if (left<0) left=0;
  if (score->len > musv_len) {
    musv_len=score->len;
    theSView->scrollbar->SetRange(0,(musv_len-sect_mus_max)*sect_len);
  }
  for (n=0;n<sclin_max;n++)
    score->lin[n]->eraseScLine(this,n,left,left+sect_mus_max+10);
  for (n=0;n<sclin_max;n++)
    score->lin[n]->drawScLine(this,n,left,left+sect_mus_max+10);
  for (n=0;n<score->len;n++) {
    score->time_mark(this,n);
    score->drawText(this,n);
    if (score->end_sect==n) break;
  }
  score->drawEnd(this);
  draw_start_stop(score,0,play_start,true);
  draw_start_stop(score,0,play_stop,false);
}

void MusicView::redraw(bool meter) {
  BRect rect(Bounds());
  float y;
  SetHighColor(cWhite);
  if (meter) {
    y=rect.top;
    FillRect(BRect(rect.left,y,rect.right,y+11)); // clear meter numbers
  }
  y=rect.top+ypos(sclin_max)-5;
  FillRect(BRect(rect.left,y,rect.right,rect.bottom)); // clear tune names
  Draw(rect);
}

ScopeBuf::ScopeBuf():occupied(-1),scope_window(0) { }

void ScopeBuf::set_buf(const int dim) {
  scope_window=dim;
  for (int n=0;n<scope_max;++n) buf[n]=new float[scope_window];
  reset();
}

void ScopeBuf::reset() {
  cur_buf=scope_max-1;
  occupied=-1;
  for (int m=0;m<scope_max;m++) for (int n=0;n<scope_window;n++) buf[m][n]=0.0;
}

int ScopeBuf::get_prev() {
  --cur_buf;
  if (occupied<scope_max-1) ++occupied;
  if (cur_buf<0) cur_buf=scope_max-1;
  return cur_buf;
}

struct ScopeView: BView {
  ScopeView(BRect rect): BView(rect,0,B_FOLLOW_RIGHT,B_WILL_DRAW) {
    SetViewColor(cBackground);
  }
  void Draw(BRect rect) {
    ScopeBuf &scbuf=appWin->scope_buf;
    if (!scbuf.scope_window) return;
    int n,m,offset;
    const int scope_gap=4;     // gap between scope windows
    float *buf;
    const float half=rect.bottom/2,
                hi=half-20,
                lo=half+20;
    BRect area;
    for (m=0; m<scope_max && m<=scbuf.occupied; ++m) {
      buf=scbuf.buf[(scbuf.cur_buf+m)%scope_max];
      offset=(scbuf.occupied-m)*(scbuf.scope_window+scope_gap);

      area.Set(offset+1,1,offset+scbuf.scope_window-1,rect.bottom-1);
      SetHighColor(cLightBlue);
      FillRect(area);

      SetPenSize(1);  // dotted line
      SetHighColor(cBlue);
      for (n=5;n<scbuf.scope_window;n+=20) {
        StrokeLine(BPoint(offset+n,hi),BPoint(offset+n+2,hi));
        StrokeLine(BPoint(offset+n,lo),BPoint(offset+n+2,lo));
      }
      // if (debug) printf("m=%d occ=%d\n",m,scbuf.occupied);

      SetHighColor(cBlack);
      MovePenTo(offset,half-20*buf[0]);
      for (n=1;n<scbuf.scope_window;++n) StrokeLine(BPoint(n+offset,half-20*buf[n]));

      area.InsetBy(-1,-1);  // boarder
      StrokeRect(area);
    }
  }
};

struct ScopeSView: StdView {
  ScopeView *scopeView;
  BScrollView *scrollView;
  ScopeSView(BRect rect): StdView(rect) {
    rect.bottom-=B_H_SCROLL_BAR_HEIGHT;
    scopeView=new ScopeView(rect);
    scrollView=new BScrollView("scope",scopeView,B_FOLLOW_RIGHT,0,true,false,B_NO_BORDER);
  }
  void draw() {
    static bool init;
    if (!init) {
      init=true;
      scrollView->ScrollBar(B_HORIZONTAL)->SetRange(0,appWin->scope_buf.scope_window * scope_max);
    }
    scopeView->Draw(scopeView->Bounds());
  }
};

struct AppView: BView {
  Array<ScoreViewStruct*,2>scViews;
  MouseAction *mouseAction;
  Question *textView;
  MeterView *meterView;
  TempoView *tempoView;
  TunesSView *tunesSView;
  ColorView *colorView;
  MusicSView *musicSView;
  ScopeSView *scopeSView;
  EditScript *editScript;
  AppView(BRect rect);
};

Score* Scores::exist_name(const char *nam) {
  if (!nam || !nam[0]) { alert("null name"); return 0; }
  for (int i=0;i<=lst_score;++i) {
    Score* sp=buf[i];
    if (!strcmp(sp->name,nam)) return sp;
  }
  return 0;
}

RedCtrl::RedCtrl(BRect rect): StdView(rect) {
  start_timbre=new HVSlider(BRect(0,0,70,ictrl_height-14),"diff/nrsin",'radn','radn',20.0,1,5,2,4);
  start_timbre->SetLimitLabels("1","5","2","4");
  start_timbre->SetValue(4);
  start_timbre->SetValueY(3);
  AddChild(start_timbre);

  timbre=new HVSlider(BRect(70,0,140,ictrl_height-14),"diff/nrsin",'rsdn','rsdn',20.0,1,5,2,4);
  timbre->SetLimitLabels("1","5","2","4");
  timbre->SetValue(3);
  timbre->SetValueY(3);
  AddChild(timbre);

  startup=new HSlider(BRect(196,0,244,0),"startup",'rest',0,5);
  startup->SetLimitLabels("0","5");
  startup->SetValue(2);
  AddChild(startup);

  start_amp=new VSlider(BRect(142,10,192,66),"start-amp",'samp',0,3);
  start_amp->SetLimitLabels("0","3");
  start_amp->SetValue(2);
  AddChild(start_amp);

  decay=new HSlider(BRect(196,42,244,0),"decay",'rede',0,5);
  decay->SetLimitLabels("0","5");
  decay->SetValue(0);
  AddChild(decay);
}

OrangeCtrl::OrangeCtrl(BRect rect): StdView(rect) {
  timbre=new HVSlider(BRect(2,0,70,ictrl_height-14),"diff/nrsin",'osdn','osdn',20.0,1,5,2,4);
  timbre->SetLimitLabels("1","5","2","4");
  timbre->SetValue(4);
  timbre->SetValueY(3);
  AddChild(timbre);

  attack=new HSlider(BRect(152,0,200,0),"attack",'orat',0,5);
  attack->SetLimitLabels("0","5");
  attack->SetValue(0);
  AddChild(attack);

  decay=new HSlider(BRect(152,42,200,0),"decay",'orde',0,5);
  decay->SetLimitLabels("0","5");
  decay->SetValue(1);
  AddChild(decay);
}

void RedCtrl::Draw(BRect rect) {
  DrawString("start wave",BPoint(0,rect.bottom-5));
  DrawString("sustain wave",BPoint(72,rect.bottom-5));
}

void OrangeCtrl::Draw(BRect rect) {
  DrawString("waveform",BPoint(0,rect.bottom-5));
}

void RedCtrl::redraw_sliders() {
  timbre->Invalidate();
  start_timbre->Invalidate();
  startup->Invalidate();
  decay->Invalidate();
  start_amp->Invalidate();
}

void OrangeCtrl::redraw_sliders() {
  timbre->Invalidate();
  attack->Invalidate();
  decay->Invalidate();
}

BlackCtrl::BlackCtrl(BRect rect): StdView(rect) {
  sub_band=new CheckBox(BPoint(90,60),"sub band",'blsu');
  AddChild(sub_band);

  fm_ctrl=new HVSlider(BRect(0,0,90,ictrl_height),"fm freq/index",0,0,30.0,0,7,0,7);
  fm_ctrl->SetLimitLabels("0","7","0","7");
  fm_ctrl->SetValue(2);
  set_fm(1);
  fm_ctrl->SetText("3.0");
  fm_ctrl->SetValueY(3);
  set_fm(2);
  fm_ctrl->SetTextY("2.0");
  fm_ctrl->modMessage=new BMessage('blfm');
  fm_ctrl->modMessageY=new BMessage('blin');
  AddChild(fm_ctrl);

  attack=new HSlider(BRect(160,0,210,0),"attack",'blaa',0,5);
  attack->SetLimitLabels("0","5");
  attack->SetValue(0);
  AddChild(attack);

  decay=new HSlider(BRect(160,slider_height,210,0),"decay",'blad',0,5);
  decay->SetLimitLabels("0","5");
  decay->SetValue(1);
  AddChild(decay);
}

void BlackCtrl::redraw_sliders() {
  fm_ctrl->SetText("%.2f",act_freq); fm_ctrl->SetTextY("%.1f",display_mod);
  fm_ctrl->Invalidate();
  attack->Invalidate();
  decay->Invalidate();
  sub_band->Invalidate();
}

BrownCtrl::BrownCtrl(BRect rect,BrownCtrl*& bc): StdView(rect) {
  bc=this;
  sub_band=new CheckBox(BPoint(90,60),"sub band",'subb');
  AddChild(sub_band);

  detune=new HSlider(BRect(95,0,145,0),"detune",'bdet',0,5);
  detune->SetLimitLabels("0","5");
  detune->SetValue(0);
  AddChild(detune);

  fm_ctrl=new HVSlider(BRect(0,0,90,ictrl_height),"fm freq/index",0,0,30.0,0,7,0,7);
  fm_ctrl->SetLimitLabels("0","7","0","7");
  fm_ctrl->SetValue(2);
  set_fm(1);
  fm_ctrl->SetText("3.0");
  fm_ctrl->SetValueY(3);
  set_fm(2);
  fm_ctrl->SetTextY("2.0");
  fm_ctrl->modMessage=new BMessage('fmfr');
  fm_ctrl->modMessageY=new BMessage('fmin');
  AddChild(fm_ctrl);

  attack=new HSlider(BRect(160,0,210,0),"attack",'brat',0,5);
  attack->SetLimitLabels("0","5");
  attack->SetValue(0);
  AddChild(attack);

  decay=new HSlider(BRect(160,slider_height,210,0),"decay",'brde',0,5);
  decay->SetLimitLabels("0","5");
  decay->SetValue(1);
  AddChild(decay);
}

void BrownCtrl::redraw_sliders() {
  fm_ctrl->SetText("%.2f",act_freq); fm_ctrl->SetTextY("%.1f",display_mod);
  fm_ctrl->Invalidate();
  attack->Invalidate();
  decay->Invalidate();
  detune->Invalidate();
  sub_band->Invalidate();
}

BlueCtrl::BlueCtrl(BRect rect): StdView(rect) {
  attack=new HSlider(BRect(5,0,55,0),"attack",'blat',0,5);
  attack->SetLimitLabels("0","5");
  attack->SetValue(0);
  AddChild(attack);
  decay=new HSlider(BRect(5,slider_height,55,0),"decay",'blde',0,5);
  decay->SetLimitLabels("0","5");
  decay->SetValue(2);
  AddChild(decay);
  p_attack=new CheckBox(BPoint(70,0),"piano attack",'piat');
  AddChild(p_attack);
  rich=new CheckBox(BPoint(70,15),"rich tone",0);
  AddChild(rich);
  chorus=new CheckBox(BPoint(70,30),"chorus",0);
  AddChild(chorus);
}

void BlueCtrl::redraw_sliders() {
  attack->Invalidate();
  decay->Invalidate();
  p_attack->Invalidate();
  rich->Invalidate();
  chorus->Invalidate();
}

GreenCtrl::GreenCtrl(BRect rect): StdView(rect) {
  tone=new VSlider(BRect(5,0,45,55),"tone",0,0,3);
  tone->SetLimitLabels("0","3");
  tone->SetValue(2);
  AddChild(tone);

  attack=new HSlider(BRect(60,0,110,0),"attack",'grat',0,5);
  attack->SetLimitLabels("0","5");
  attack->SetValue(0);
  AddChild(attack);
  decay=new HSlider(BRect(60,slider_height,110,0),"decay",'grde',0,5);
  decay->SetLimitLabels("0","5");
  decay->SetValue(1);
  AddChild(decay);
}

void GreenCtrl::redraw_sliders() {
  tone->Invalidate();
  attack->Invalidate();
  decay->Invalidate();
}

PurpleCtrl::PurpleCtrl(BRect rect): StdView(rect) {
  int n,
      left=0;
  static char *labels[]={ "1","2","3","6","10" };
  static int init[][harm_max]={{ 3,0,0,1,2 },{ 2,3,3,1,1 }};
  for (n=0;n<harm_max;++n) {
    st_harm[n]=new VSlider(BRect(left,0,left+14,55),labels[n],'puah',0,3);
    st_harm[n]->SetValue(init[0][n]);
    AddChild(st_harm[n]);
    left+=14;
  }
  left+=15;
  for (n=0;n<harm_max;++n) {
    harm[n]=new VSlider(BRect(left,0,left+14,55),labels[n],'purp',0,3);
    harm[n]->SetValue(init[1][n]);
    AddChild(harm[n]);
    left+=14;
  }
  set_st_hs_ampl(init[0]);
  set_hs_ampl(init[1]);
  start_dur=new HSlider(BRect(left+10,0,left+60,0),"startup",'pusu',0,5);
  start_dur->SetLimitLabels("0","5");
  start_dur->SetValue(2);
  AddChild(start_dur);
}

void PurpleCtrl::redraw_sliders() {
  for (int n=0;n<harm_max;++n) {
    st_harm[n]->Invalidate();
    harm[n]->Invalidate();
  }
  start_dur->Invalidate();
}

void PurpleCtrl::Draw(BRect rect) {
  DrawString("startup",BPoint(0,rect.bottom-14));
  DrawString("harmonics",BPoint(0,rect.bottom-4));
  DrawString("sustain",BPoint(5*14+15,rect.bottom-14));
  DrawString("harmonics",BPoint(5*14+15,rect.bottom-4));
}

ShowSampled::ShowSampled(BRect rect): StdView(rect) {
  small_font=new BFont(be_plain_font);
  small_font->SetSize(10);
}

void ShowSampled::Draw(BRect rect) {
  RawData *raw;
  SetHighColor(cBlack);
  if (raw_data_okay) { 
    DrawString("sampled instruments:",BPoint(2,10));
    SetFont(small_font);
    for (int n=0;n<colors_max;n++) {
      raw=RAW+col2wav_nr(colors[n]);
      SetHighColor(color(colors[n]));
      if (raw->file_name) DrawString(raw->file_name,BPoint(120,10*n+10));
    }
    SetFont(appFont);
  }
  else
    DrawString("(wave files not yet read)",BPoint(2,10));
}

struct InfoText : StdView {
  UpdText *updTexts[3];
  bool sco_modif,  // any score modified?
       scr_modif;  // script modified?
  InfoText(BRect rect):
      StdView(rect) {
    updTexts[0]=new UpdText(this,BPoint(10,22)); // .sco file
    updTexts[1]=new UpdText(this,BPoint(10,46)); // .scr file
    updTexts[2]=new UpdText(this,BPoint(54,58)); // measure nr
  }
  void draw_modif(int nr,bool yes) {
    BRect r(0,0,8,8);
    switch (nr) {
      case 0: r.OffsetTo(Bounds().right-10,2); break;
      case 1: r.OffsetTo(Bounds().right-10,26); break;
    }
    if (yes) SetHighColor(cRed);
    else SetHighColor(cForeground);
    FillRect(r);
  }
  void set_modif(int nr,bool on) {
    switch (nr) {
      case 0: sco_modif=on; break;
      case 1: scr_modif=on; break;
    }
    draw_modif(nr,on);
  }
  void Draw(BRect rect) {
    SetHighColor(cBlack);
    DrawString("score file:",BPoint(2,10));
    DrawString("script file:",BPoint(2,34));
    DrawString("measure:",BPoint(2,58));
    for (int n=0;n<3;++n)
      updTexts[n]->write(0);
    draw_modif(0,sco_modif);
    draw_modif(1,scr_modif);
  }
};


AppView::AppView(BRect frame): BView(frame,0,B_FOLLOW_ALL,0) {
  SetViewColor(cBackground);
  SetLowColor(cBackground);
  BRect rect(0,0,x_off+x_gap+sect_max*sect_len+score_info_len+2,scview_vmax),
        rect2;
  for (int n=0;n<2;n++) {
    scViews[n]=new ScoreViewStruct(rect,n);
    if (n<=scores.lst_score) scViews[n]->assign_score(scores[n]);
    else scViews[n]->scv_view->score=0;
    AddChild(scViews[n]->scroll_view->scrollbar);
    AddChild(scViews[n]->scv_view);
    AddChild(scViews[n]->scv_text);
    rect.top+=scview_vmax+4;
    rect.bottom+=scview_vmax+4;
  }

  mouseAction=new MouseAction(BPoint(0,rect.top));
  AddChild(mouseAction);

  rect.Set(0,view_vmax-scview_vmax-16,view_hmax-editwin_width,view_vmax);
  musicSView=new MusicSView(rect);
  AddChild(musicSView->scrollbar);
  AddChild(musicSView->musicView);

  editScript=new EditScript(BRect(view_hmax+2-editwin_width,rect.top,view_hmax+2,view_vmax-15));
  AddChild(editScript->scrollview);

  rect.Set(view_hmax-buttons_left,2,view_hmax-buttons_left+10,0);
  Button *but;
  AddChild(new Button(rect,"save...",'save',B_FOLLOW_RIGHT)); rect.top+=but_dist;

  AddChild(new Button(rect,"load...",'load',B_FOLLOW_RIGHT)); rect.top+=but_dist;

  but=new Button(rect,"new...",'new ',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but); rect.top+=but_dist;

  but=new Button(rect,"copy...",'cp_t',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but); rect.top+=but_dist;

  but=new Button(rect,"rename...",'rnam',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but); rect.top+=but_dist;

  but=new PictButton(BRect(rect.left,rect.top,rect.left+20,0),eUp,'mvup',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but);
  but=new PictButton(BRect(rect.left+22,rect.top,rect.left+42,0),eDown,'mvdo',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but); rect.top+=but_dist;

  but=new Button(rect,"clear",'clr ',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but);
  rect.top+=but_dist;

  but=new Button(rect,"remove",'rm_t',B_FOLLOW_RIGHT);
  but->textColor=cDarkBlue; AddChild(but); rect.top+=but_dist;
  
  but=new Button(rect,"script...",'scr ',B_FOLLOW_RIGHT);
  AddChild(but);
  rect.top+=but_dist;

  AddChild(new Button(rect,"save script...",'sv_s',B_FOLLOW_RIGHT)); rect.top+=but_dist;
  AddChild(new Button(rect,"mod timing",'modt',B_FOLLOW_RIGHT)); rect.top+=but_dist;
  AddChild(new Button(rect,"cmd...",'cmd ',B_FOLLOW_RIGHT)); rect.top+=but_dist;
  AddChild(new Button(rect,"run script",'run ',B_FOLLOW_RIGHT)); rect.top+=but_dist;

  but=new Button(rect,"play",'plmu',B_FOLLOW_RIGHT);
  but->textColor=cRed; AddChild(but);
  but=new Button(BRect(rect.left+but->dx()+2,rect.top,0,0),"stop",'stop',B_FOLLOW_RIGHT);
  AddChild(but); rect.top+=but_dist;

  float top=0;

  rect.Set(view_hmax-rbutview_left,top,view_hmax-rbutview_left+100,top+107);
  tunesSView=new TunesSView(rect);
  AddChild(tunesSView->scrollView);

  rect2.Set(rect.right+4,top,rect.right+58,top+3*radiob_dist+15);
  meterView=new MeterView(rect2);
  AddChild(meterView);

  top=rect2.bottom+4;
  rect2.Set(rect2.left,top,rect2.right,top+45);
  tempoView=new TempoView(rect2);
  AddChild(tempoView);

  top=rect2.bottom+4;
  rect2.Set(rect2.left,top,view_hmax-buttons_left-2,top+colors_max*radiob_dist+15);
  colorView=new ColorView(rect2);
  AddChild(colorView);

  top=rect.bottom+2;
  appWin->chords=new CheckBox(BPoint(rect.left+2,top),"chords",'chd ',B_FOLLOW_RIGHT);
  AddChild(appWin->chords);

  top+=checkbox_height;
  appWin->note_dist=new CheckBox(BPoint(rect.left+2,top),"note distances",'ndis',B_FOLLOW_RIGHT);
  AddChild(appWin->note_dist);

  top+=checkbox_height;
  appWin->solo=new CheckBox(BPoint(rect.left+2,top),"solo voice",0,B_FOLLOW_RIGHT);
  AddChild(appWin->solo);

  top+=checkbox_height;
  appWin->draw_col=new CheckBox(BPoint(rect.left+2,top),"draw with color",0,B_FOLLOW_RIGHT);
  AddChild(appWin->draw_col);

  top+=checkbox_height;
  appWin->use_raw=new CheckBox(BPoint(rect.left+2,top),"sampled notes",'uraw',B_FOLLOW_RIGHT);
  AddChild(appWin->use_raw);

  top+=checkbox_height;
  appWin->dumpwav=new CheckBox(BPoint(rect.left+2,top),"create wave file",0,B_FOLLOW_RIGHT);
  AddChild(appWin->dumpwav);

  top+=checkbox_height;
  appWin->midi_output=new CheckBox(BPoint(rect.left+2,top),"create midi file",0,B_FOLLOW_RIGHT);
  AddChild(appWin->midi_output);

  top+=checkbox_height;
  appWin->no_set=new CheckBox(BPoint(rect.left+2,top),"ignore set cmd's",0,B_FOLLOW_RIGHT);
  AddChild(appWin->no_set);

  top+=checkbox_height;
  appWin->con_mkeyb=new CheckBox(BPoint(rect.left+2,top),"connect USB midi-keyboard",'c_mk',B_FOLLOW_RIGHT);
  AddChild(appWin->con_mkeyb);

  top+=checkbox_height+3;
  rect.Set(rect.left,top,view_hmax-buttons_left-2,top+38);
  textView=new Question(rect);
  AddChild(textView);

  // instrument control views
  top+=42;
  rect.Set(view_hmax-rbutview_left,top,view_hmax,top+ictrl_height);
  AddChild(appWin->black_control=new BlackCtrl(rect));
  appWin->act_instr_ctrl=appWin->black_control;
  AddChild(appWin->red_control=new RedCtrl(rect));       appWin->red_control->Hide();
  AddChild(appWin->orange_control=new OrangeCtrl(rect)); appWin->orange_control->Hide();
  AddChild(appWin->blue_control=new BlueCtrl(rect));     appWin->blue_control->Hide();
  AddChild(appWin->green_control=new GreenCtrl(rect));   appWin->green_control->Hide();
  AddChild(appWin->purple_control=new PurpleCtrl(rect)); appWin->purple_control->Hide();
  AddChild(new BrownCtrl(rect,appWin->brown_control));   appWin->brown_control->Hide();
  AddChild(appWin->show_sampled=new ShowSampled(rect));  appWin->show_sampled->Hide();

  top=rect.bottom+4;
  rect.Set(view_hmax-rbutview_left,top,view_hmax-94,top+70);
  scopeSView=new ScopeSView(rect);
  AddChild(scopeSView->scrollView);

  appWin->scope_range=new CheckBox(BPoint(rect.right+4,top),"scope: all",0,B_FOLLOW_RIGHT);
  AddChild(appWin->scope_range);
  top+=20;

  appWin->info_text=new InfoText(BRect(rect.right+4,top,view_hmax,top+60));
  AddChild(appWin->info_text);
}

struct Encode {
  char save_buf[50];
  char* set_meter(int m) {
    sprintf(save_buf,"%um%d",eSetMeter,m);
    return save_buf;
  }
  char* score_start(int score_nr,int note_col,int score_ampl,int flat_notes,int sharp_notes,int endsect) {
    sprintf(save_buf,"%uc%da%df%ds%d",eScore_start,note_col,score_ampl,flat_notes,sharp_notes);
    if (endsect>0) sprintf(save_buf+strlen(save_buf),"e%d",endsect);
    return save_buf;
  }
  char* play_note(int lnr,int snr,int dur,int sign,int stacc_sampled,
                  int col,int ampl,int dlnr,int dsnr,int del_s,int del_e) {
    sprintf(save_buf,"%uL%dN%dd%di%ds%dc%da%d ",ePlayNote,lnr,snr,dur,sign,stacc_sampled,col,ampl);
    if (del_s || del_e) sprintf(save_buf+strlen(save_buf)-1,"p%d,%dD%d,%d ",dlnr,dsnr,del_s,del_e);
    else if (dlnr) sprintf(save_buf+strlen(save_buf)-1,"p%d,%d ",dlnr,dsnr);
    return save_buf;
  }
  bool decode(FILE *in,Score*& scp) {
    ScSection *sp;
    int res;
    uint opc=0;
    opc=atoi(save_buf);
    switch (opc) {
      case eSetMeter:
        if (sscanf(save_buf,"%*um%d",&appWin->act_meter)!=1) {
          alert("bad code: %s",save_buf); return false;
        }
        break;
      case eScore_start: {
          int n,lnr,
              flatn,sharpn;
          if (!scores.in_range(scores.lst_score+1)) return false;
          scp=scores[++scores.lst_score];
          if (scp) scp->reset();
          else scp=scores[scores.lst_score]=new Score(0,sect_max,0);
          res=sscanf(save_buf,"%*uc%da%df%ds%de%d",&scp->ncol,&scp->nampl,&flatn,&sharpn,&scp->end_sect);
          if (res==4||res==5) {
            scp->set_signs(flatn,sharpn);
            scp->signs_mode= flatn ? eFlat : sharpn ? eSharp : 0;
            if (res==5)    // end_sect > nop
              scp->check_len(scp->end_sect+1);
          }
          else if (res!=2) {
            alert("bad code: %s",save_buf);
            return false;
          };
          fscanf(in,"%s",textbuf);
          scp->name=strdup(textbuf);
        }
        break;
      case eScore_end:
        if (sscanf(save_buf,"%*ue%d",&scp->end_sect)!=1) { alert("bad code: %s",save_buf); return false; }
        scp->check_len(scp->end_sect+1);
        break;
      case ePlayNote: {
          int ind,snr,dur,sign,stacc_sampl,col,ampl,dlnr,dsnr,del_s,del_e;
          res=sscanf(save_buf,"%*uL%dN%dd%di%ds%dc%da%dp%d,%dD%d,%d",
                     &ind,&snr,&dur,&sign,&stacc_sampl,&col,&ampl,&dlnr,&dsnr,&del_s,&del_e);
          switch (res) {
            case 7: dlnr=dsnr=del_s=del_e=0; break;
            case 9: del_s=del_e=0; break;
            case 11: break;
            default: alert("bad code: %s",save_buf); return false;
          }
          scp->check_len(snr+dur); // 1 extra
          sp=scp->get_section(ind,snr);
          sp->del_start=del_s;
          for (int i=dur;;) {
            sp->cat=ePlay;
            sp->note_col=col;
            sp->sign=sign;
            sp->stacc=0;
            sp->sampled=0;
            sp->note_ampl=ampl;
            if (--i<=0) break;
            ++sp;
          }
          sp->stacc= stacc_sampl & 1;
          sp->sampled= (stacc_sampl>>1) & 1;
          sp->port_dlnr=abs(dlnr);
          sp->dlnr_sign= dlnr>=0;
          sp->port_dsnr=dsnr;
          sp->del_end=del_e;
        }
        break;
      default: alert("unknown opcode %d",opc); return false;
    }
    return true;
  }
};

void set_filetype(const char *file,const char *type) {
  BFile bfile(file,0);
  if (bfile.InitCheck()==B_OK) {
    BNodeInfo ni(&bfile);
    if (ni.SetType(type)!=B_OK) alert("filetype %s not set",file);
    bfile.Unset();
  }
  else alert("no icon for %s",file);
}

void AppWindow::unfocus_textviews() {  // prevents unwanted key-down effect in textviews
  static BTextView *tv1=appView->editScript->textview,
                   *tv2=appView->textView->textControl;
  tv1->MakeFocus(false);
  tv2->MakeFocus(false);
}

bool AppWindow::find_score(Score *sc,ScoreViewStruct*& svs) {
  for (int n=0;n<2;++n) {
    svs=appView->scViews[n];
    if (svs->scv_view->score && svs->scv_view->score==sc)
      return true;
  }
  return false;
}

bool AppWindow::save(const char *file) {
  FILE *tunes;
  int i,n;
  Encode enc;
  Score *scp;
  ScSection *sect,
            *first_sect;
  int scorenr,
      snr,lnr,
      flatn,sharpn;
  if ((tunes=fopen(file,"w"))==0) { alert("file '%s' not writable",file); return false; }
  fputs(enc.set_meter(act_meter),tunes);
  for (scorenr=0;scorenr<=scores.lst_score;++scorenr) {
    scp=scores[scorenr];
    flatn=sharpn=0;
    for (lnr=0;;++lnr) {
      switch (scp->lin[lnr]->note_sign) {
        case eLo: flatn|=1; break;
        case eHi: sharpn|=1; break;
      }
      if (lnr==6) break;
      flatn<<=1; sharpn<<=1;
    }
    fprintf(tunes,"\n%s %s ",
      enc.score_start(scorenr,scp->ncol,scp->nampl,flatn,sharpn,scp->end_sect),scp->name);
    for (lnr=0;lnr<sclin_max;lnr++) {
      for (snr=0;snr<scp->len-1;) {   // 1 extra for end
        if (scp->end_sect==snr) break;
        sect=scp->get_section(lnr,snr);
        switch (sect->cat) {
          case ePlay:
            first_sect=sect;
            for (i=1;;++i,++sect) {
              if (sect->sampled) { n=2; break; }
              if (sect->stacc) { n=1; break; }
              if (!eq(sect+1,first_sect)) { n=0; break; }
            }
            fputs(enc.play_note(lnr,snr,i,first_sect->sign,n,
                                first_sect->note_col,first_sect->note_ampl,
                                sect->dlnr_sign ? sect->port_dlnr : -sect->port_dlnr,sect->port_dsnr,
                                first_sect->del_start,sect->del_end),tunes);
            snr+=i;
            break;
          case eSilent:
            ++snr;
            break;
          default:
            alert("save %u?",sect->cat);
            ++snr;
        }
      }
    }
  }
  putc('\n',tunes); // Unix tools need this
  fclose(tunes);
  set_filetype(file,tunes_mime);

  return true;
}

bool AppWindow::restore(const char *file) {
  FILE *in;
  bool result=true;
  Encode enc;
  if ((in=fopen(file,"r"))==0) {
    alert("file '%s' not found",file);
    return false;
  }
  scores.lst_score=-1;
  Score* scp=0;
  for (;;) {
    if (fscanf(in,"%s",enc.save_buf)<1) break;
    if (isdigit(enc.save_buf[0]) && isalpha(enc.save_buf[1])) {
      if (!enc.decode(in,scp)) { result=false; break; }
    }
    else {
      alert("bad .sco code: %s",enc.save_buf);
      result=false;
      break;
    }
  }
  fclose(in);
  return result;
}

void AppWindow::set_ctrl_sliders() {
  black_control->redraw_sliders();
  red_control->redraw_sliders();
  brown_control->redraw_sliders();
  green_control->redraw_sliders();
  purple_control->redraw_sliders();
  blue_control->redraw_sliders();
  orange_control->redraw_sliders();
  appView->tempoView->redraw_sliders();
  appView->colorView->redraw_sliders();
}

int32 start_sampler(void*) {
  new Sampler(); // does not return
}  

int32 start_usb(void*) {
  roster.Start(); // never returns
}

AppWindow::AppWindow(AppWindow*& appWindow,char *appf,char *inf):
    BWindow(
      BRect(20,24,20+view_hmax,24+view_vmax),
      "the A'dam Music Composer - 1.14",
      B_DOCUMENT_WINDOW,B_ASYNCHRONOUS_CONTROLS|B_OUTLINE_RESIZE),
    app_file(appf),
    act_tune(0),
    input_file(inf),
    act_score(nop),
    act_color(eBlack),
    act_meter(8),
    act_action(nop),
    prev_mess(0),
    act_tempo(11),
    stop_requested(false),
    repeat(0),
    appView(0),   // so crash if used prematurely
    chordsWin(0),
    ndistWin(0),
    scope_buf(),
    mn_end(0),
    active_scoreCtrl(new RButtonAM1LampCtrl('acts')),
    scoreBuf(new Score("sc buf",sect_max,0)),
    cur_score(0) {
  appWindow=this;
  if (inf) {
    char *s=input_file.get_ext();
    if (s && !strcmp(s,".scr")) {
      script_file.cpy(input_file.s);
      input_file.new_ext(".sco");
      if (!restore(input_file.s)) inf=0;
    }
    else if (!restore(input_file.s)) inf=0;
  }
  else {
    input_file.cpy("the.sco");
    scores[0]=new Score("tune1",sect_max,0);
    scores[1]=new Score("tune2",sect_max,0);
    scores.lst_score=1;
  }
  appView=new AppView(Bounds());
  appView->SetFont(appFont);
  AddChild(appView);
  if (inf) info_text->updTexts[0]->write(input_file.strip_dir());

  if (script_file.s[0]) {
    if (read_script(script_file.s,appView->musicSView->musicView))
      info_text->updTexts[1]->write(script_file.strip_dir());
  }

  AddHandler(&appView->textView->handler);  
  msgr=new BMessenger(&appView->textView->handler);
  sampler_id=spawn_thread(start_sampler,"sampler",B_REAL_TIME_PRIORITY,0);
  resume_thread(sampler_id);
}

bool AppWindow::QuitRequested() {
  be_app->PostMessage(B_QUIT_REQUESTED);
  return true;
}

int key2num(key_info *key) {
  if (key->key_states[2])
    switch (key->key_states[2]) {
      case 32: return 1; case 16: return 2; case 8: return 3;
      case 4: return 4; case 2: return 5; case 1: return 6;
    }
  else if (key->key_states[3])
    switch (key->key_states[3]) {
      case 128: return 7; case 64: return 8; case 32: return 9;
      case 16: return 0;
    }
  return -1;
}

int key2char(key_info *key) {
  if (key->key_states[10]==1 && key->key_states[12]==32) return 'ud';
  if (key->key_states[10]==1) return 'up';    // up arrow
  if (key->key_states[12]==32) return 'do';   // down arrow
  if (key->key_states[7]==4) return 'sel';    // s
  if (key->key_states[5]==4) return 'uns';    // u
  if (key->key_states[6]==128) return 'prta'; // p
  if (key->key_states[10]==32) return 'move'; // m
  if (key->key_states[9]==2) return 'copy';   // c
  if (key->key_states[9]==16) return 'kp_a';  // shift, keep score active
  if (key->key_states[7]==8) return 'all';    // a
  if (key->key_states[8]==16) return 'keep';  // k
  return 0;
}

BView* col2ctrl(int col) {
  switch (col) {
    case eBlack: return appWin->black_control;
    case eRed: return appWin->red_control;
    case eOrange: return appWin->orange_control;
    case eBlue: return appWin->blue_control;
    case eGreen: return appWin->green_control;
    case ePurple: return appWin->purple_control;
    case eBrown: return appWin->brown_control;
    default: alert("col2ctrl col=%d",col); return 0;
  }
}

void AppWindow::check_reset_as() { // do not reset act_score if 'k' key pressed
  key_info key;
  get_key_info(&key);
  if (key2char(&key)!='kp_a') {
    active_scoreCtrl->reset();
    act_score=nop;
  }
}

void ScoreView::modify_sel(int mes) {
  SLList_elem<SectData> *sd;
  ScSection *sect;
  for (sd=selected.sd_list.lis;sd;sd=sd->nxt) {
    sect=score->lin[sd->d.lnr]->sect + sd->d.snr;
    switch (mes) {
      case 'uns': sect->sel=false; break;
      case 'rcol': sect->note_col=appWin->act_color; break;
      case 'amp+': if (sect->note_ampl<ampl_max) ++sect->note_ampl; break;
      case 'amp-': if (sect->note_ampl>1) --sect->note_ampl; break;
      case 'del ':
        if (sect->port_dlnr) sect->drawPortaLine(this,sd->d.snr,sd->d.lnr,true);
        sect->reset();
        break;
    }
    sect->drawSect(this,sd->d.snr,sd->d.lnr);
  }
  switch (mes) {
    case 'del ':
    case 'uns':
      selected.reset();
      score->drawEnd(this);
      break;
  }
}

void ScoreViewStruct::reset() {
  scv_view->score=0;
  if (zoomWin) { 
    if (scv_text->zoom->value) {
      scv_text->zoom->SetValue(false);
      zoomWin->Hide();
    }
  }
}

bool AppWindow::read_tunes(const char* file) {
  static TunesView *tv=appView->tunesSView->tunesView;
  tv->reset(scores.lst_score);
  if (!restore(file)) return false;  // sets scores.lst_score
  int n;
  for (n=0;n<2;++n)
    appView->scViews[n]->reset();
  for (n=0;n<=scores.lst_score;++n) {
    if (n<2) appView->scViews[n]->assign_score(scores[n]);
    scores[n]->rbut=tv->ctrl->AddButton(tv,tv->top,scores[n]->name,n);
    tv->top.y += radiob_dist;
  }
  for (n=0;n<2;++n)
    appView->scViews[n]->invalidate();
  appView->meterView->redraw();
  return true;
}

void Scores::swap(int ind1,int ind2) {
  char *label=buf[ind1]->name;
  RadioButton *rb=buf[ind1]->rbut;
  buf[ind1]->rbut->reLabel(buf[ind2]->name);
  buf[ind1]->rbut=buf[ind2]->rbut;
  buf[ind2]->rbut->reLabel(label);
  buf[ind2]->rbut=rb;
  Score *sc=buf[ind1];
  buf[ind1]=buf[ind2]; buf[ind2]=sc;
}

void Scores::remove(Score *sc) {
  int sc_ind=get_index(sc),
      ind;
  Score *sc1,*sc2;
  if (appWin->act_tune==sc) {
    appWin->act_tune=0;
    sc->rbut->SetValue(false);
  }
  RadioButton *lst_but=buf[lst_score]->rbut;
  lst_but->Remove();
  for (ind=lst_score-1;ind>=sc_ind;--ind) {
    sc1=buf[ind];
    sc2=buf[ind+1];
    sc2->rbut=sc1->rbut;
    sc2->rbut->reLabel(sc2->name);
  }
  for (ind=sc_ind;ind<lst_score;++ind) {
    sc1=buf[ind];
    sc2=buf[ind+1];
    if (appWin->act_tune==sc2) sc1->rbut->SetValue(true);
    buf[ind]=sc2;
  }
  buf[lst_score]=sc; // instead of deleting
  --lst_score;
}

ScoreViewStruct *find_svs_ptr(BMessage *mes) {
  void *ptr; 
  mes->FindPointer("",&ptr);
  return reinterpret_cast<ScoreViewStruct*>(ptr);
}

StoredNote* Selected::store_selection(int& lst_note) {
  if (!sd_list.lis)
    return 0;
  int n,n1,sgn;
  static StoredNote buffer[stored_notes_max];
  SLList_elem<SectData> *sd;
  ScSection *sect;

  lst_note=0;
  if (inv) {
    sd_list.invert();
    inv=false;
  }
  for (sd=selected.sd_list.lis;sd;sd=sd->nxt) {
    sect=sv->score->lin[sd->d.lnr]->sect + sd->d.snr;
    if (sect->cat==ePlay) {
      for (n=0;
           (sect+n)->cat==ePlay && (sect+n)->sel && (sect+n)->sign==sect->sign;
           ++n) (sect+n)->cat=ePlay_x;
      if (lst_note>=stored_notes_max) {
        alert("store_sel: more then %d notes",stored_notes_max);
        return buffer;
      }
      StoredNote &b=buffer[lst_note++];
      b.lnr=sd->d.lnr;
      b.snr=sd->d.snr;
      b.dur=n;
      sgn=0;
      switch (sect->sign) {
        case eHi: sgn=1; break;
        case eLo: sgn=-1; break;
      }
      b.sign= sgn;
    }
  }
  for (sd=selected.sd_list.lis;sd;sd=sd->nxt)  // reset
    sv->score->lin[sd->d.lnr]->sect[sd->d.snr].cat=ePlay;
  return buffer;
}

void Score::fill_from_buffer(StoredNote *notes,int dim) {
  int n,n1,
      new_lnr,
      new_sign;
  ScSection* sect;
  reset();
  for (n=0;n<dim;++n) {
    StoredNote& sn=notes[n];
    new_lnr=sn.lnr;
    new_sign=0;
    switch (sn.sign) {
      case -1:
        if (make_lower[sn.lnr%7]) { ++new_lnr; new_sign=0; } else new_sign=eLo;
        break;
      case 1:
        if (make_higher[sn.lnr%7]) { --new_lnr; new_sign=0; } else new_sign=eHi;
        break;
    }
    check_len(sn.snr+sn.dur);
    sect=lin[new_lnr]->sect + sn.snr;
    for (n1=0;n1<sn.dur;++n1) {
      sect[n1].cat=ePlay;
      sect[n1].note_ampl=4;
      sect[n1].sign=new_sign;
    }
  }
}

Settings::Settings(int m):meter(m) { }

void AppWindow::MessageReceived(BMessage* mes) {
  int n,n1,n2;
  bool ok;
  int32 mval,
        cur_mess=mes->what;
  static int32 act_tune_ind;
  void *ptr;
  const char *txt;
  MusicView *mv;
  ScoreView *sv,*sv1;
  static ScoreViewStruct *cur_svs;
  ScoreViewStruct *svs;
  TunesView *tv;
  BTextView *textv;
  Score *sc;
  Question *quest=appView->textView;
  static Str command;
  switch (cur_mess) {
    case STORE_SEL: {
        BMessage reply(CONTENTS_SEL);
        StoredNote *csel=selected.store_selection(n);
        Settings settings(act_meter);
        reply.AddData(SELD_NOTES,B_OBJECT_TYPE,csel,n*sizeof(StoredNote));
        reply.AddData(SETTINGS,B_OBJECT_TYPE,&settings,sizeof(Settings));
        mes->SendReply(&reply);
      }
      break;
    case SEND_NOTES: {  // notes from acm-scheme
        StoredNote* notes;
        ssize_t bytes=0;
        mes->FindData(SENT_NOTES,B_OBJECT_TYPE,(const void**)(&notes),&bytes);
        sc=scores.new_score("from-scheme");
        sc->ncol=act_color;
        sc->fill_from_buffer(notes,bytes/sizeof(StoredNote));
        tv=appView->tunesSView->tunesView;
        sc->rbut=tv->ctrl->AddButton(tv,tv->top,sc->name,scores.lst_score);
        tv->top.y += radiob_dist;
        if (act_score>nop) {
          svs=appView->scViews[act_score];
          if (selected.sv==svs->scv_view) selected.restore_sel();
          svs->assign_score(sc);
          svs->invalidate();
          check_reset_as();
        }
      }
      break;
    case 'acts': // from active_scoreCtrl
      mes->FindInt32("",&act_score);
      break;
    case 'setm':  // from meterView
      mes->FindInt32("",&mval);
      if (mval!=act_meter) {
        act_meter=mval;
        for (n=0;n<2;++n) appView->scViews[n]->scv_view->redraw(true);
        appView->musicSView->musicView->redraw(true);
      }
      break;
    case 'tune': // from tunesView
      mes->FindInt32("",&act_tune_ind);
      act_tune=scores[act_tune_ind];
      if (act_score>nop) {
        if (!act_tune) break;
        if (find_score(act_tune,svs)) {
          alert("tune %s already assigned",act_tune->name);
          break;
        }
        else {
          svs=appView->scViews[act_score];
          sv=svs->scv_view;
          if (selected.sv==sv)
            selected.restore_sel();
          svs->assign_score(act_tune);
          svs->invalidate();
        }
        check_reset_as();
      }
      break;
    case 'modt':  // modify timing in script
      alerts=0;
      mv=appView->musicSView->musicView;
      if (script_file.s[0])
        modify_script(appView->editScript->textview,mv->play_start,mv->play_stop);
      else
        alert("no script file");
      break;
    case 'scrm':  // script modified
      info_text->set_modif(1,true);
      break;
    case 'play':  // play a score
      alerts=0;
      stop_requested=false;
      scope_buf.reset();
      cur_svs=find_svs_ptr(mes);
      repeat=&cur_svs->scv_text->set_repeat->value;
      sv=cur_svs->scv_view;
      cur_score=sv->score;
      if (cur_score) sampler->playScore(sv->play_start,sv->play_stop);
      break;
    case 'rept':  // set repeat
      break;
    case 'repl':  // replay
      scope_buf.reset();
      keyboard_tune.nxt_turn();
      if (cur_score) {
        if (cur_score->is_music) {
          mv=appView->musicSView->musicView;
          sampler->playScore(mv->play_start,mv->play_stop);
        }
        else {
          sv=cur_svs->scv_view;
          sampler->playScore(sv->play_start,sv->play_stop);
        }
      }
      break;
    case 'puah': {  // update attack harmonics purple instr
        static int st_harmonics[harm_max];
        for(n=0;n<harm_max;++n)
          st_harmonics[n]=purple_control->st_harm[n]->value;
        purple_control->set_st_hs_ampl(st_harmonics);
      }
      break;
    case 'purp': {  // update purple instr
        static int harmonics[harm_max];
        for(n=0;n<harm_max;++n)
          harmonics[n]=purple_control->harm[n]->value;
        purple_control->set_hs_ampl(harmonics);
      }
      break;
    case 'pusu':  // startup duration purple instr
      purple_control->set_start_dur();
      break;
    case 'grat':  // attack green instrument
      green_control->set_attack();
      break;
    case 'grde':  // decay green instrument
      green_control->set_decay();
      break;
    case 'blaa':  // attack black instrument
      black_control->set_attack();
      break;
    case 'brat':  // attack brown instrument
      brown_control->set_attack();
      break;
    case 'bdet':  // detune brown instrument
      brown_control->set_detune();
      break;
    case 'blad':  // decay black instrument
      black_control->set_decay();
      break;
    case 'brde':  // decay brown instrument
      brown_control->set_decay();
      break;
    case 'rest':  // startup red instrument
      red_control->set_startup();
      break;
    case 'samp':  // start-ampl red instrument
      red_control->set_start_amp();
      break;
    case 'orat':  // attack orange instrument
      orange_control->set_attack();
      break;
    case 'rede':  // decay red instrument
      red_control->set_decay();
      break;
    case 'orde':  // decay orange instrument
      orange_control->set_decay();
      break;
    case 'uraw':  // sampled waves
      act_instr_ctrl->Hide();
      if (use_raw->value)
        (act_instr_ctrl=show_sampled)->Show();
      else
        (act_instr_ctrl=col2ctrl(act_color))->Show();
      break;
    case 'c_mk':  // connect midi keyboard
      if (con_mkeyb->value) {
        if (!mk_usb_id) {
          mk_usb_id=spawn_thread(start_usb,"usb",B_NORMAL_PRIORITY,0);
          resume_thread(mk_usb_id);
        }
        mk_connected=true;
      }
      else
        mk_connected=false;
      break;
    case 'blat':  // attack blue instrument
      blue_control->set_attack();
      break;
    case 'blde':  // decay blue instrument
      blue_control->set_decay();
      break;
    case 'piat':  // from blue_control, piano attack
      blue_control->set_piano();
      break;
    case 'dmod':  // display mode of scoreview
      svs=find_svs_ptr(mes);
      sv=svs->scv_view;
      switch (svs->scv_text->display_mode->value) {
        case 0: sv->draw_mode=eColorValue; break;
        case 1: sv->draw_mode=eAmplValue; break;
      }
      sv->redraw(false);
      break;
    case 'ampl':  // set score amplitude
      svs=find_svs_ptr(mes);
      sv=svs->scv_view;
      if (sv->score) {
        sv->score->nampl=svs->scv_text->sc_ampl->value;
        for (n=0;n<sclin_max;++n) 
          for (n1=0;n1<sv->score->len;++n1)
            sv->score->get_section(n,n1)->note_ampl=sv->score->nampl;
        if (sv->draw_mode==eAmplValue) sv->redraw(false);
      }
      break;
    case 'plmu':  // play musicView
      alerts=0;
      stop_requested=false;
      mv=appView->musicSView->musicView;
      if (dumpwav->value) {
        if (!(dumpwav_okay=init_dump_wav(wave_out_file)))
          alert("%s not opened",wave_out_file);
      }
      if (midi_output->value) {
        if (!(midiout_okay=midi_out.init(midi_out_file)))
          alert("%s not opened",midi_out_file);
        midi_output->SetValue(false);  
      }
      cur_score=mv->score;
      sampler->playScore(mv->play_start,mv->play_stop);
      break;
    case 'save':
      txt=input_file.s;
      quest->text->bgColor=cPink;
      quest->text->write("save as:");
      quest->textControl->SetText(txt);
      if (prev_mess==cur_mess) {
        prev_mess=0;
        if (save(txt)) {
          quest->text->write("saved");
          info_text->set_modif(0,false);
        }
      }
      else {
        prev_mess=cur_mess;
        msgr->SendMessage(mes);
      }
      break;
    case 'sv_s':    // save script
      txt=script_file.s[0] ? script_file.s : "the.scr";
      quest->text->bgColor=cPink;
      quest->text->write("save script as:");
      quest->textControl->SetText(txt);
      if (prev_mess==cur_mess) {
        prev_mess=0;
        if (save_script(txt)) {
          quest->text->write("script saved");
          info_text->set_modif(1,false);
        }
      }
      else {
        prev_mess=cur_mess;
        msgr->SendMessage(mes);
      }
      break;
    case 'new ':  // new score
      quest->text->bgColor=cPink;
      quest->text->write("new tune:");
      quest->textControl->SetText("no-name");
      msgr->SendMessage(mes);
      break;
    case 'cp_t':  // copy tune
      if (!act_tune) {
        alert("copy: no tune selected");
        break;
      }
      quest->text->bgColor=cPink;
      quest->text->write("copy to tune:");
      quest->textControl->SetText("no-name");
      msgr->SendMessage(mes);
      break;
    case 'rnam':   // rename tune
      if (!act_tune) break;
      quest->text->bgColor=cPink;
      quest->text->write("rename to:");
      quest->textControl->SetText(act_tune->name);
      msgr->SendMessage(mes);
      break;
    case 'mvup':    // move tune up in tunesView
      if (!act_tune || act_tune_ind==0) break;
      scores.buf[act_tune_ind-1]->rbut->SetValue(true);
      scores.swap(act_tune_ind,act_tune_ind-1);
      --act_tune_ind;
      break;
    case 'mvdo':    // move tune down
      if (!act_tune || act_tune_ind==scores.lst_score) break;
      scores.buf[act_tune_ind+1]->rbut->SetValue(true);
      scores.swap(act_tune_ind,act_tune_ind+1);
      ++act_tune_ind;
      break;
    case 'load':    // load score file
      txt=input_file.s;
      quest->text->bgColor=cPink;
      quest->text->write("load:");
      quest->textControl->SetText(txt);
      if (prev_mess==cur_mess) {
        prev_mess=0;
        if (read_tunes(txt)) {
          quest->text->write("loaded");
          info_text->updTexts[0]->write(input_file.strip_dir());
        }
        else info_text->updTexts[0]->write("");
      }
      else {
        prev_mess=cur_mess;
        msgr->SendMessage(mes);
      }
      break;
    case 'clr ':    // clear a score
      if (act_score>nop) {
        sv=appView->scViews[act_score]->scv_view;
        if (sv->score) {
          if (selected.sv==sv)
            selected.reset();
          sv->score->reset();
          sv->score->ncol=act_color;
          sv->Invalidate();
        }
        check_reset_as();
      }
      else {
        mv=appView->musicSView->musicView;
        if (mv->score->lst_sect>=0) {
          mv->reset(true);
          mv->redraw(true); // clear start and stop sign 
        }
      }
      break;
    case 'rm_t':    // remove tune
      if (!act_tune) break;
      if (find_score(act_tune,svs)) {
        if (selected.sv==svs->scv_view)
          selected.reset();
        svs->scv_view->score=0;
        svs->reset();
        svs->scv_view->Invalidate();
        svs->scv_text->sc_name->write("");
      } 
      scores.remove(act_tune);
      appView->tunesSView->tunesView->top.y -= radiob_dist;
      break;
    case 'run ':    // run script
      textv=appView->editScript->textview;
      textv->MakeFocus(false);
      alerts=0;
      mv=appView->musicSView->musicView;
      mv->reset(false);
      mv->redraw(false);
      run_script(textv->Text(),mv);
      break;
    case 'scr ':    // read script
      if (!script_file.s[0]) {
        script_file.cpy(input_file.s);
        script_file.new_ext(".scr");
      }
      quest->text->bgColor=cPink;
      quest->text->write("script:");
      quest->textControl->SetText(script_file.s);
      if (prev_mess==cur_mess) {
        prev_mess=0;
        alerts=0;
        mv=appView->musicSView->musicView;
        mv->reset(false);
        mv->redraw(false);
        if (read_script(script_file.s,mv)) {
          quest->text->write("read");
          info_text->updTexts[1]->write(script_file.strip_dir());
        }
        else info_text->updTexts[1]->write("");
      }
      else {
        prev_mess=cur_mess;
        msgr->SendMessage(mes);
      }
      break;
    case 'cmd ':  // command
      quest->text->bgColor=cPink;
      quest->text->write("command:");
      quest->textControl->SetText(command.s);
      if (prev_mess==cur_mess) {
        prev_mess=0;
        if (read_script(0,appView->musicSView->musicView)) quest->text->write("read");
      }
      else {
        prev_mess=cur_mess;
        msgr->SendMessage(mes);
      }
      break;
    case 'tmpo':  // from tempoView
      act_tempo=appView->tempoView->tempo->value;
      appView->tempoView->show_val();
      break;
    case 'radn':  // from red_control, attack diff/nrsin
      red_control->set_start_timbre();
      break;
    case 'osdn':  // from orange_control, diff/nrsin
      orange_control->set_timbre();
      break;
    case 'rsdn':  // from red_control, sustain diff/nsin
      red_control->set_timbre();
      break;
    case 'blfm':  // from black_control, fm freq
      black_control->set_fm(1);
      black_control->fm_ctrl->SetText("%.2f",black_control->act_freq);
      black_control->fm_ctrl->UpdateText();
      break;
    case 'fmfr':  // from brown_control, fm freq
      brown_control->set_fm(1);
      brown_control->fm_ctrl->SetText("%.2f",brown_control->act_freq);
      brown_control->fm_ctrl->UpdateText();
      break;
    case 'blin':  // from black_control, fm index
      black_control->set_fm(2);
      black_control->fm_ctrl->SetTextY("%.1f",black_control->display_mod);
      black_control->fm_ctrl->UpdateText();
      break;
    case 'fmin':  // from brown_control, fm index
      brown_control->set_fm(2);
      brown_control->fm_ctrl->SetTextY("%.1f",brown_control->display_mod);
      brown_control->fm_ctrl->UpdateText();
      break;
    case 'blsu':  // from black_control, sub band freq
      black_control->set_fm(1);
      black_control->fm_ctrl->SetText("%.2f",black_control->act_freq);
      black_control->fm_ctrl->UpdateText();
      break;
    case 'subb':  // from brown_control, sub band freq
      brown_control->set_fm(1);
      brown_control->fm_ctrl->SetText("%.2f",brown_control->act_freq);
      brown_control->fm_ctrl->UpdateText();
      break;
    case 'msta':  // from mouseAction
      mes->FindInt32("",&act_action);
      break;
    case 'colr':  // from colorView
      mes->FindInt32("",&act_color);
      if (act_score>nop) {
        sv=appView->scViews[act_score]->scv_view;
        if (sv->score) {
          sv->score->ncol=act_color;
          for (n1=0;n1<sclin_max;++n1) {
            ScLine *lin=sv->score->lin[n1];
            for (n2=0;n2<sv->score->len;++n2) {
              lin->sect[n2].note_col=act_color;
              lin->sect[n2].nxt_note=0;  // needed if 2 tunes are joined
            }
          }
          sv->redraw(false);
        }
        check_reset_as();
      }
      act_instr_ctrl->Hide();
      if (use_raw->value)
        (act_instr_ctrl=show_sampled)->Show();
      else
        (act_instr_ctrl=col2ctrl(act_color))->Show();
      break;
    case 'csnr': // current section nr
      mes->FindInt32("",&mval);
      info_text->updTexts[2]->write("%d",mval/act_meter);
      break;
    case 'scop':
      appView->scopeSView->draw();
      break;
    case 'upsl':  // from BufferProc()
      set_ctrl_sliders();
      if (dumpwav_okay) {
        if (close_dump_wav()) alert("wave file %s created",wave_out_file);
        else alert("%s not closed",wave_out_file);
        dumpwav_okay=false;
        dumpwav->SetValue(false);  
      }
      break;
    case 'stop':  // from stop button
      stop_requested=true;
      break;
    case 'c_kt':  // copy keyboard tune
      sc=scores.new_score("keyboard");
      tv=appView->tunesSView->tunesView;
      sc->rbut=tv->ctrl->AddButton(tv,tv->top,sc->name,scores.lst_score);
      tv->top.y += radiob_dist;
      sc->copy_keyb_tune(keyboard_tune);
      keyboard_tune.reset();
      break;
    case 'ctrl':  // from quest->handler
      quest->textControl->reset();
      txt=quest->textControl->Text();
      prev_mess=0;
      switch (quest->handler.mode) {
        case 'sv_s':  // quest->handler.mode
          script_file.cpy(txt);
          if (save_script(txt)) {
            quest->text->write("saved");
            info_text->updTexts[1]->write(script_file.strip_dir()); // maybe txt was changed
          }
          else info_text->updTexts[1]->write("");
          break;
        case 'save':  // quest->handler.mode
          input_file.cpy(txt);
          if (save(txt)) {
            quest->text->write("saved");
            info_text->updTexts[0]->write(input_file.strip_dir()); // maybe txt was changed
          }
          else info_text->updTexts[0]->write("");
          break;

        case 'load':  // quest->handler.mode
          selected.reset();
          input_file.cpy(txt);
          if (read_tunes(txt)) {
            quest->text->write("loaded");
            info_text->updTexts[0]->write(input_file.strip_dir());
          }
          else info_text->updTexts[0]->write("");
          break;
        case 'new ':  // quest->handler.mode
          sc=scores.new_score(txt);
          sc->ncol=act_color;
          tv=appView->tunesSView->tunesView;
          sc->rbut=tv->ctrl->AddButton(tv,tv->top,sc->name,scores.lst_score);
          tv->top.y += radiob_dist;
          quest->text->write("tune added");
          if (act_score>nop) {
            svs=appView->scViews[act_score];
            if (selected.sv==svs->scv_view) selected.restore_sel();
            svs->assign_score(sc);
            svs->invalidate();
            check_reset_as();
          }
          break;
        case 'rnam':   // quest->handler.mode
          if (!act_tune) break;
          act_tune->name=strdup(txt);
          act_tune->rbut->reLabel(act_tune->name);
          if (find_score(act_tune,svs))
            svs->scv_text->sc_name->write(act_tune->name);
          quest->text->write("tune renamed");
          break;
        case 'cp_t':    // quest->handler.mode
          if (!act_tune) break;
          sc=scores.exist_name(txt);
          if (sc) {
            sc->reset();
            sc->copy(act_tune);
            if (find_score(sc,svs)) {
              svs->scv_text->sc_ampl->SetValue(sc->nampl);
              svs->invalidate();
            }
          }
          else {
            sc=scores.new_score(txt);
            sc->ncol=act_color;
            tv=appView->tunesSView->tunesView;
            sc->rbut=tv->ctrl->AddButton(tv,tv->top,sc->name,scores.lst_score);
            tv->top.y += radiob_dist;
            sc->copy(act_tune);
          }
          if (act_score>nop) {
            svs=appView->scViews[act_score];
            if (selected.sv==svs->scv_view) selected.restore_sel();
            svs->assign_score(sc);
            svs->invalidate();
            check_reset_as();
          }
          quest->text->write("copied");
          break;
        case 'scr ':  // quest->handler.mode
          alerts=0;
          mv=appView->musicSView->musicView;
          if (mv->score->lst_sect>0) {
            if (script_file==txt) mv->reset(false); else mv->reset(true);
            mv->redraw(false);
          }
          script_file.cpy(txt);
          if (read_script(txt,mv)) {
            quest->text->write("read");
            info_text->updTexts[1]->write(script_file.strip_dir());
          }
          else info_text->updTexts[1]->write("");
          break;
        case 'cmd ':  // quest->handler.mode
          alerts=0;
          command.cpy(txt);
          if (read_script(0,appView->musicSView->musicView)) quest->text->write("read");
          break;
        case 0:
          alert("question mode = 0");
          break;
        default: alert("unk question mode");
      }
      break;
    case 'zomC':  // zoom window close
      svs=find_svs_ptr(mes);
      svs->scv_text->zoom->SetValue(false);
      svs->zoomWin->Hide();
      break;
    case 'zom ':  // zoom scoreview
      svs=find_svs_ptr(mes);
      if (svs->scv_text->zoom->value) {
        if (svs->scv_view->play_start > svs->scv_view->play_stop) {
          alert("zoom: stop must be > 0 and > start");
          svs->scv_text->zoom->SetValue(false);
          break;
        }
        BPoint top(Frame().left+svs->scv_text->Frame().right+10,
                   Frame().top  +svs->scv_text->Frame().top);
        if (!svs->zoomWin)
          svs->zoomWin=new ZoomWindow(top,svs);
        else
          svs->zoomWin->MoveTo(top);
        svs->zoomWin->Show();
      }
      else
        if (svs->zoomWin) svs->zoomWin->Hide();
      break;
    case 'chdC':   // close chords window
      chords->SetValue(false);
      chordsWin->Hide();
      break;
    case 'chd ':   // chords checkbox
      if (chords->value) {
        BPoint top(Frame().right+10,Frame().top);
        if (!chordsWin)
          chordsWin=new ChordsWindow(top);
        else
          chordsWin->MoveTo(top);
        chordsWin->Show();
      }
      else
        if (chordsWin) chordsWin->Hide();
      break;
    case 'ndiC':   // close note dist window
      note_dist->SetValue(false);
      ndistWin->Hide();
      break;
    case 'ndis':   // note distances checkbox
      if (note_dist->value) {
        mv=appView->musicSView->musicView;
        if (mv->play_start > mv->play_stop) {
          alert("note dist: stop must be > 0 and > start");
          note_dist->SetValue(false);
          break;
        }
        BPoint top(Frame().right+10,Frame().top);
        if (!ndistWin) ndistWin=new NoteDistWindow(top);
        else ndistWin->MoveTo(top);
        ndistWin->set(mv->play_start,mv->play_stop,mv->score);
        ndistWin->Show();
      }
      else {
        if (ndistWin) ndistWin->Hide();
      }
      break;
    case 'chkn':   // from chords window
      if (act_score>nop) {
        sv=appView->scViews[act_score]->scv_view;
        if (sv->score) {
          selected.restore_sel();
          selected.sv=sv;
          sv->score->put_chord(sv); // chord will be selected
        }
        check_reset_as();
      }
      break;
    case 'setk':  // from chords window, set key button
      if (act_score>nop) {
        sv=appView->scViews[act_score]->scv_view;
        if (sv->score) {
          sv->score->tone2signs();
          sv->SetHighColor(cWhite);
          sv->FillRect(BRect(0,0,x_off-1,sv->Bounds().bottom)); // clear old signs
          sv->drawSigns();
        }
        check_reset_as();
      }
      break;
    case 'frck':  // from chords window, force key button
      if (act_score>nop) {
        sv=appView->scViews[act_score]->scv_view;
        if (sv->score) {
          int nxt;
          for (n1=0;n1<sclin_max;++n1) {
            ScLine *lin=sv->score->lin[n1];
            for (n2=0;n2<sv->score->len;++n2) {
              lin->sect[n2].sign=lin->note_sign;
              for (nxt=lin->sect[n2].nxt_note;nxt;nxt=mn_buf[nxt].nxt_note)
                mn_buf[nxt].sign=lin->note_sign;
            }
          }
          sv->redraw(false);
        }
        check_reset_as();
      }
      break;
    case 'uns':      // unselect
    case 'rcol':     // re-color selected
    case 'amp+':     // amplitude+1 selected
    case 'amp-':     // amplitude-1 selected
    case 'del ':
      if (selected.sv) selected.sv->modify_sel(cur_mess);
      appView->mouseAction->reset();
      break;
    default:
      BWindow::MessageReceived(mes);
  }
}

Score* Scores::new_score(const char *name) {
  if (!in_range(lst_score+1)) return buf[0];
  ++lst_score;
  Score* sc=buf[lst_score];
  if (!sc) sc=buf[lst_score]=new Score(strdup(name),sect_max,0);
  else { sc->reset(); sc->name=strdup(name); }
  return sc;
}

void get_token(Str& str,const char *text,char* delim,int& pos) {
  if (debug) printf("get_token: str=[%s] delim=[%s] pos=%d\n",str.s,delim,pos);
  str.strtok(text,delim,pos);
}

int read_time(Str& str,const char *text,int& pos) {   // e.g. time:2.3
  get_token(str,text," .\n;",pos);
  bool neg= str.s[0]=='-';
  int nr=atoi(str.s) * appWin->act_meter;
  if (str.ch=='.') {
    get_token(str,text," \n;",pos);
    if (neg) nr-=atoi(str.s); else nr+=atoi(str.s);
  }
  return nr;
}

void read_times(const char *text,Str& str,int& pos,Array<int,times_max>& tim,const int stop) {   // e.g. time:2.3,4
  int nr;
  for (int n=0;;++n) {
    get_token(str,text," .\n;,",pos);
    nr=atoi(str.s) * appWin->act_meter;
    if (str.ch=='.') {
      get_token(str,text," \n;,",pos);
      nr+=atoi(str.s);
    }
    tim[n]=nr;
    if (str.ch!=',') { tim[n+1]=stop; return; }
  }
}

bool AppWindow::exec_cmd(ScInfo& info) {
  int n;
  switch (info.tag) {
    case eText:
      break;
    case eTempo:
      act_tempo=info.n;
      break;
    case eRed_att_timbre:
      red_control->start_timbre->value=info.n2[0];
      red_control->start_timbre->valueY=info.n2[1];
      red_control->set_start_timbre();
      break;
    case eRed_timbre:
      red_control->timbre->value=info.n2[0];
      red_control->timbre->valueY=info.n2[1];
      red_control->set_timbre();
      break;
    case eOrange_timbre:
      orange_control->timbre->value=info.n2[0];
      orange_control->timbre->valueY=info.n2[1];
      orange_control->set_timbre();
      break;
    case eRed_attack:
      red_control->startup->value=info.n;
      red_control->set_startup();
      break;
    case eRed_stamp:
      red_control->start_amp->value=info.n;
      red_control->set_start_amp();
      break;
    case eRed_decay:
      red_control->decay->value=info.n;
      red_control->set_decay();
      break;
    case ePurple_a_harm: {
        static int st_harmonics[harm_max];
        for (n=0;n<harm_max;++n) purple_control->st_harm[n]->value = st_harmonics[n] = info.n5[n];
        purple_control->set_st_hs_ampl(st_harmonics);
      }
      break;
    case ePurple_s_harm: {
        static int harmonics[harm_max];
        for (n=0;n<harm_max;++n) purple_control->harm[n]->value = harmonics[n] = info.n5[n];
        purple_control->set_hs_ampl(harmonics);
      }
      break;
    case ePurple_attack:
      purple_control->start_dur->value=info.n;
      purple_control->set_start_dur();
      break;
    case eBlue_attack:
      blue_control->attack->value=info.n;
      blue_control->set_attack();
      break;
    case eBlue_decay:
      blue_control->decay->value=info.n;
      blue_control->set_decay();
      break;
    case eBlue_piano:
      blue_control->p_attack->value=info.b;
      blue_control->set_piano();
      break;
    case eBlue_rich:
      blue_control->rich->value=info.b;
      break;
    case eBlue_chorus:
      blue_control->chorus->value=info.b;
      break;
    case eBlack_mod:
      black_control->fm_ctrl->value=info.n2[0];
      black_control->fm_ctrl->valueY=info.n2[1];
      black_control->set_fm(1);
      black_control->set_fm(2);
      break;
    case eBlack_attack:
      black_control->attack->value=info.n;
      black_control->set_attack();
      break;
    case eBlack_decay:
      black_control->decay->value=info.n;
      black_control->set_decay();
      break;
    case eBrown_mod:
      brown_control->fm_ctrl->value=info.n2[0];
      brown_control->fm_ctrl->valueY=info.n2[1];
      brown_control->set_fm(1);
      brown_control->set_fm(2);
      break;
    case eBrown_detune:
      brown_control->detune->value=info.n;
      brown_control->set_detune();
      break;
    case eBrown_attack:
      brown_control->attack->value=info.n;
      brown_control->set_attack();
      break;
    case eBrown_decay:
      brown_control->decay->value=info.n;
      brown_control->set_decay();
      break;
    case eBlack_subband:
      black_control->sub_band->value=info.b;
      black_control->set_fm(1);
      break;
    case eBrown_subband:
      brown_control->sub_band->value=info.b;
      brown_control->set_fm(1);
      break;
    case eGreen_tone:
      green_control->tone->value=info.n;
      break;
    case eGreen_attack:
      green_control->attack->value=info.n;
      green_control->set_attack();
      break;
    case eGreen_decay:
      green_control->decay->value=info.n;
      green_control->set_decay();
      break;
    case eOrange_attack:
      orange_control->attack->value=info.n;
      orange_control->set_attack();
      break;
    case eOrange_decay:
      orange_control->decay->value=info.n;
      orange_control->set_decay();
      break;
    case eBlack_loc:
      appView->colorView->stereo[0]->value=info.n;
      break;
    case eRed_loc:
      appView->colorView->stereo[1]->value=info.n;
      break;
    case eGreen_loc:
      appView->colorView->stereo[2]->value=info.n;
      break;
    case eBlue_loc:
      appView->colorView->stereo[3]->value=info.n;
      break;
    case eBrown_loc:
      appView->colorView->stereo[4]->value=info.n;
      break;
    case ePurple_loc:
      appView->colorView->stereo[5]->value=info.n;
      break;
    case eOrange_loc:
      appView->colorView->stereo[6]->value=info.n;
      break;
    default:
      alert("exec_cmd: unknown tag %u",info.tag);
      return false;
  }
  return true;
}

void set_wave(Str &str,const char *text,int &pos,const char* col,
                           int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," ,\n;",pos);
  if (str.ch!=',') { alert("bad %s syntax",col); return; }
  n=atoi(str.s);
  if (n<1 || n>5) { alert("bad %s value: %d",col,n); n=2; }
  info.n2[0]=n;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (n<2 || n>5) { alert("bad %s value: %d",col,n); n=3; }
  info.n2[1]=n;
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_slider(int N,Str &str,const char *text,int &pos,const char* col,
                           int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  n=atoi(str.s);
  if (!isdigit(str.s[0])) { alert("bad %s value: %s",col,str.s); n=0; }
  if (n<0 || n>N) { alert("bad %s value: %d",col,n); n=0; }
  info.tag=en;
  info.n=n;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_bool(Str &str,const char *text,int &pos,const char* col,
                         int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  if (str=="on") info.b=true;
  else if (str=="off") info.b=false;
  else { alert("bad %s value: %s",col,str.s); info.b=false; }
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

void set_loc(Str &str,const char *text,int &pos,const char* word,
                           int en,Array<int,times_max>times,MusicView *musV) {
  int n;
  ScInfo info;
  get_token(str,text," \n;",pos);
  if (str=="left") info.n=0;
  else if (str=="mid") info.n=1;
  else if (str=="right") info.n=2;
  else { alert("bad %s value: %s",word,str.s); info.n=1; }
  info.tag=en;
  for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
}

bool eq(char *word,char *prefix,char *all,char *&s) {
/* word="attack", prefix="red", all="red-attack"
   or:
   word="red-attack", all="red-attack"
*/
  s=all;
  int plen=strlen(prefix);
  if (!strcmp(word,all) ||
      plen && !strncmp(prefix,all,plen) && !strcmp(word,all+plen+1))
    return true;
  return false;
}

bool AppWindow::read_script(const char *script,MusicView* musV) {
  // calls add_copy(), which draws sections, meter and tune names in musicView
  int mode;
  if (!script) mode=eString;
  else {
    mode=eFile;
    FILE *in=0;
    if (!(in=fopen(script,"r"))) {
      alert("'%s' unknown",script); return false;
    }
    appView->editScript->read_scriptf(in);
    fclose(in);
    set_filetype(script,script_mime);
  }
  const char *text= mode==eFile ? appView->editScript->textview->Text():
                                  appView->textView->textControl->Text();
  return run_script(text,musV);
}

bool AppWindow::save_script(const char *script) {
  FILE *out=0;
  if (!(out=fopen(script,"w"))) {
      alert("'%s' not writable",script); return false;
  }
  appView->editScript->save_scriptf(out);
  fclose(out);
  return true;
}

bool AppWindow::run_script(const char *text,MusicView* musV) {
  Score *from=0,
        *to=0;
  Str str;
  int n,n1,
      cmd,
      pos,thePos,
      line_nr,
      lst_ch;
  char *s,
       prefix[10];
  ScInfo info;
  Array<int,times_max> times;
  ScoreViewBase *display=0;
  ScoreViewStruct *svs;
  str.cmt_ch='#';
  appView->editScript->textview->reset();
  for (pos=thePos=0,line_nr=1;;) {
    lst_ch=str.ch;
    get_token(str,text," \n;",pos);
    if (!str.s[0] && str.ch==0) break;
    if (!str.s[0]) { ++line_nr; continue; }
    if (debug) printf("cmd:%s\n",str.s);
    if ((cmd=eAdd,str=="add") ||          // from scores to music
        (cmd=eTake,str=="take") ||        // from scores to scoreBuf
        (cmd=eTake_nc,str=="take-nc")) {  // from scores to scoreBuf, no clear
      switch (cmd) {
        case eAdd:
          thePos=pos;
          from=scoreBuf; to=musV->score; display=musV;
          break;
        case eTake:
          from=0; to=scoreBuf; display=0;
          to->reset();
          break;
        case eTake_nc:
          from=0; to=scoreBuf; display=0;
          break;
      }
      times[0]=to->lst_sect+1;
      times[1]=eo_arr;
      int ampl=nop,
          start=0,
          stop=nop,
          shift=0,
          raise=0;
      for (;;) {
        if (strchr("\n;",str.ch)) {
          if (!from) { alert("error in script"); return false; }
          int act_stop = stop>0 ? stop : from->end_sect>0 ? from->end_sect : from->len;
          switch (cmd) {
            case eAdd: break;
            case eTake: // to = scoreBuf
              to->name=from->name; // needed for command "add"
              to->ncol=from->ncol;
              to->nampl=from->nampl;
              to->signs_mode=from->signs_mode;
              for (n=0;n<sclin_max;++n)
                to->lin[n]->note_sign=from->lin[n]->note_sign;
              break;
            case eTake_nc: break;
          }
          for (n=0;times[n]!=eo_arr;++n)
            to->add_copy(from,start,act_stop,times[n],ampl,shift,raise,display);
          if (cmd==eAdd && lst_ch=='\n') appView->editScript->report_meas(thePos,times[0]);
          break;
        }
        get_token(str,text," :\n;",pos);
        if (debug) printf("add/take cmd: %s ch=[%c]\n",str.s,str.ch);
        if (str.ch==':') {
          if (str=="time") read_times(text,str,pos,times,eo_arr);
          else if (str=="rt") {
            times[0]=to->lst_sect+1+read_time(str,text,pos);
            times[1]=eo_arr;
          }
          else if (str=="ampl") { get_token(str,text," \n;",pos); ampl=atoi(str.s); }
          else if (str=="from") start=read_time(str,text,pos);
          else if (str=="to") stop=read_time(str,text,pos);
          else if (str=="shift") { get_token(str,text," \n;",pos); shift=atoi(str.s); }
          else if (str=="raise") { get_token(str,text," \n;",pos); raise=atoi(str.s); }
          else { alert("unk option '%s'",str.s); return false; }
        }
        else {
          from=scores.exist_name(str.s);
          if (!from) { alert("tune '%s' unknown",str.s); return false; }
        }
      }
    }
    else if (str=="put") {      // from scoreBuf to scores
      if (str.ch!=' ') { alert("tune name missing after put cmd"); return false; }
      get_token(str,text," \n;",pos);
      to=scores.exist_name(str.s);
      if (to) {
        to->reset();
        to->copy(scoreBuf);
        if (find_score(to,svs)) svs->invalidate();
      }
      else {
        if (!scores.in_range(scores.lst_score+1))  // alert by Array
          return false;
        to=scores[++scores.lst_score];
        if (!to)
          to=scores[scores.lst_score]=new Score(strdup(str.s),sect_max,0);
        to->copy(scoreBuf);
        to->name=strdup(str.s);
        TunesView *tv=appView->tunesSView->tunesView;
        to->rbut=tv->ctrl->AddButton(tv,tv->top,to->name,scores.lst_score);
        tv->top.y += radiob_dist;
      }
    }
    else if (str=="set" || str=="SET") {
      prefix[0]=0;
      times[0]=musV->score->lst_sect+1; // default time
      times[1]=eo_arr;
      for (;;) {
        get_token(str,text," :\n;",pos);
        if (debug) printf("set cmd:%s\n",str.s);
        if (str.ch==':') {
          if (str=="time")
            read_times(text,str,pos,times,eo_arr);
          else if (str=="rt") {
            times[0]=musV->score->lst_sect+1+read_time(str,text,pos);
            times[1]=eo_arr;
          }
          else if (str=="tempo") {
            get_token(str,text," \n;",pos);
            info.tag=eTempo;
            info.n=atoi(str.s)/10;
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (eq(str.s,prefix,"red-start-wave",s))
            set_wave(str,text,pos,s,eRed_att_timbre,times,musV);
          else if (eq(str.s,prefix,"red-sustain-wave",s))
            set_wave(str,text,pos,s,eRed_timbre,times,musV);
          else if (eq(str.s,prefix,"orange-wave",s))
            set_wave(str,text,pos,s,eOrange_timbre,times,musV);
          else if (eq(str.s,prefix,"red-decay",s))
            set_slider(5,str,text,pos,s,eRed_decay,times,musV);
          else if (eq(str.s,prefix,"red-startup",s))
            set_slider(5,str,text,pos,s,eRed_attack,times,musV);
          else if (eq(str.s,prefix,"red-start-amp",s))
            set_slider(3,str,text,pos,s,eRed_stamp,times,musV);
          else if (eq(str.s,prefix,"green-attack",s))
            set_slider(5,str,text,pos,s,eGreen_attack,times,musV);
          else if (eq(str.s,prefix,"green-decay",s))
            set_slider(5,str,text,pos,s,eGreen_decay,times,musV);
          else if (eq(str.s,prefix,"green-tone",s))
            set_slider(3,str,text,pos,s,eGreen_tone,times,musV);
          else if (eq(str.s,prefix,"blue-attack",s))
            set_slider(5,str,text,pos,s,eBlue_attack,times,musV);
          else if (eq(str.s,prefix,"blue-decay",s))
            set_slider(5,str,text,pos,s,eBlue_decay,times,musV);
          else if (eq(str.s,prefix,"blue-piano",s))
            set_bool(str,text,pos,s,eBlue_piano,times,musV);
          else if (eq(str.s,prefix,"blue-rich",s))
            set_bool(str,text,pos,s,eBlue_rich,times,musV);
          else if (eq(str.s,prefix,"blue-chorus",s))
            set_bool(str,text,pos,s,eBlue_chorus,times,musV);
          else if ((eq(str.s,prefix,"black-fm",s))) {
            get_token(str,text," ,\n;",pos);
            if (str.ch!=',') { alert("bad %s syntax",s); return false; }
            n=atoi(str.s);
            if (n<0 || n>7) { alert("bad %s value: %d",s,n); n=0; }
            info.n2[0]=n;
            get_token(str,text," \n;",pos);
            n=atoi(str.s);
            if (n<0 || n>7) { alert("bad %s value: %d",s,n); n=0; }
            info.n2[1]=n;
            info.tag=eBlack_mod;
            for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
          }
          else if (eq(str.s,prefix,"black-decay",s))
            set_slider(5,str,text,pos,s,eBlack_decay,times,musV);
          else if (eq(str.s,prefix,"black-attack",s))
            set_slider(5,str,text,pos,s,eBlack_attack,times,musV);
          else if (eq(str.s,prefix,"black-subband",s))
            set_bool(str,text,pos,s,eBlack_subband,times,musV);
          else if (eq(str.s,prefix,"brown-fm",s)) {
            get_token(str,text," ,\n;",pos);
            if (str.ch!=',') { alert("bad %s syntax",s); return false; }
            n=atoi(str.s);
            if (n<0 || n>7) { alert("bad %s value: %d",s,n); n=0; }
            info.n2[0]=n;
            get_token(str,text," \n;",pos);
            n=atoi(str.s);
            if (n<0 || n>7) { alert("bad %s value: %d",s,n); n=0; }
            info.n2[1]=n;
            info.tag=eBrown_mod;
            for (n=0;times[n]!=eo_arr;++n) musV->upd_info(times[n],info);
          }
          else if (eq(str.s,prefix,"brown-decay",s))
            set_slider(5,str,text,pos,s,eBrown_decay,times,musV);
          else if (eq(str.s,prefix,"brown-detune",s))
            set_slider(5,str,text,pos,s,eBrown_detune,times,musV);
          else if (eq(str.s,prefix,"brown-attack",s))
            set_slider(5,str,text,pos,s,eBrown_attack,times,musV);
          else if (eq(str.s,prefix,"brown-subband",s))
            set_bool(str,text,pos,s,eBrown_subband,times,musV);
          else if (eq(str.s,prefix,"purple-start-harm",s)) {
            info.tag=ePurple_a_harm;
            for (n=0;n<harm_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=4) { alert("%s: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1<0 || n1>3) { alert("bad %s value: %d",s,n1); n1=0; }
              info.n5[n]=n1;
            }
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (eq(str.s,prefix,"purple-sustain-harm",s)) {
            info.tag=ePurple_s_harm;
            for (n=0;n<harm_max;++n) {
              get_token(str,text,", \n;",pos);
              if (str.ch!=',' && n!=4) { alert("%s: no comma",s); return false; }
              n1=atoi(str.s);
              if (n1<0 || n1>3) { alert("bad %s value: %d",s,n1); n1=0; }
              info.n5[n]=n1;
            }
            for (n=0;times[n]!=eo_arr;++n)
              musV->upd_info(times[n],info);
          }
          else if (eq(str.s,prefix,"purple-startup",s)) 
            set_slider(5,str,text,pos,s,ePurple_attack,times,musV);
          else if (eq(str.s,prefix,"orange-attack",s))
            set_slider(5,str,text,pos,s,eOrange_attack,times,musV);
          else if (eq(str.s,prefix,"orange-decay",s))
            set_slider(5,str,text,pos,s,eOrange_decay,times,musV);
          else if (eq(str.s,prefix,"black-loc",s))
            set_loc(str,text,pos,s,eBlack_loc,times,musV);
          else if (eq(str.s,prefix,"red-loc",s))
            set_loc(str,text,pos,s,eRed_loc,times,musV);
          else if (eq(str.s,prefix,"green-loc",s))
            set_loc(str,text,pos,s,eGreen_loc,times,musV);
          else if (eq(str.s,prefix,"blue-loc",s))
            set_loc(str,text,pos,s,eBlue_loc,times,musV);
          else if (eq(str.s,prefix,"brown-loc",s))
            set_loc(str,text,pos,s,eBrown_loc,times,musV);
          else if (eq(str.s,prefix,"purple-loc",s))
            set_loc(str,text,pos,s,ePurple_loc,times,musV);
          else if (eq(str.s,prefix,"orange-loc",s))
            set_loc(str,text,pos,s,eOrange_loc,times,musV);
          else {
            alert("bad option '%s' at set cmd, line %d",str.s,line_nr);
            return false;
          }
          if (strchr("\n;",str.ch)) break;
        }
        else if (str=="black" || str=="red" || str=="green" ||
                 str=="blue" || str=="brown" || str=="purple" || str=="orange") {
          strcpy(prefix,str.s);
        }
        else {
          alert("unk option %s at set cmd, line %d",str.s,line_nr);
          return false;
        }
        if (alerts>max_alerts) return false;
      }
    }
    else if (str=="exit") break;
    else { alert("unk cmd '%s' in script, line %d",str.s,line_nr); return false; }
    if (str.ch==0) break;
    if (str.ch=='\n') ++line_nr;
  }
  return true;
}

void Selected::restore_sel() {
  SLList_elem<SectData> *sd;
  ScSection *sec;
  for (sd=sd_list.lis;sd;sd=sd->nxt) {
    sec=sv->score->get_section(sd->d.lnr,sd->d.snr);
    sec->sel=false;
    sec->drawSect(sv,sd->d.snr,sd->d.lnr);
  }
  reset();
}

void ScoreView::select_all(bool all) {
  int lnr,snr;
  const int end= score->end_sect>nop ? score->end_sect : score->len,
            actcol=appWin->act_color;
  ScSection *sect;
  ScLine *lin;
  
  for (lnr=0;lnr<sclin_max;++lnr) {
    lin=score->lin[lnr];
    for (snr=0;snr<end;++snr) {
      sect=lin->sect+snr;
      if (sect->cat==ePlay && (all || sect->note_col==actcol)) {
        selected.insert(lnr,snr);
        sect->sel=true;
        sect->drawSect(this,snr,lnr);
      }
    }
  }
}

void ScoreView::select_column(int snr) {
  ScSection *sect;
  for (int lnr=0;lnr<sclin_max;++lnr) {
    sect=score->lin[lnr]->sect+snr;
    if (sect->cat==ePlay) {
      selected.insert(lnr,snr);
      sect->sel=true;
      sect->drawSect(this,snr,lnr);
    }
  }
}

void ZoomView::MouseDown(BPoint where) {
  ScoreView *sv=theVS->scv_view;
  if (!sv->score) return;
  SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);

  const int lnr=linenr(where.y);
  int snr,pos;
  sectnr(where.x,snr,pos);
  if (lnr>=sclin_max || snr<0 || snr>=sv->score->len) return;
  ScLine *cur_line=sv->score->lin[lnr];
  ScSection *const sect=cur_line->sect+snr;
  if (sect->cat!=ePlay || sect->sampled) return;
  ScSection *const prv_sect= snr>0 && (sect-1)->cat==ePlay ? sect-1 : 0,
            *const nxt_sect= (sect+1)->cat==ePlay ? sect+1 : 0;
  int y=ypos(lnr);
  BPoint start,end;
  uint32 mouse_but;
  GetMouse(&where,&mouse_but);
  switch (mouse_but) {
    case (B_PRIMARY_MOUSE_BUTTON):
      sect->del_start=(sect->del_start+1) % subdiv;
      endpoints(snr,left,start,end,y);
      if (prv_sect && prv_sect->del_end>=sect->del_start)  // shorten end of previous section?
        prv_sect->del_end=sect->del_start;
      sect->drawZoomPlaySect(this,start,end,prv_sect,nxt_sect); // draw this section, redraw previous
      break;
    case (B_SECONDARY_MOUSE_BUTTON):
      sect->del_end=(sect->del_end+1) % subdiv;
      endpoints(snr,left,start,end,y);
      if (nxt_sect && nxt_sect->del_start<=sect->del_end)  // shorten next section?
        nxt_sect->del_start=sect->del_end;
      sect->drawZoomPlaySect(this,start,end,0,nxt_sect);
      break;
  }
}

void ScoreView::MouseDown(BPoint where) {
  state=eIdle;
  if (!score) return;
  SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
  uint32 mouse_but;
  GetMouse(&where,&mouse_but);
  const int lnr=linenr(where.y);
  int snr=sectnr(where.x);
  if (lnr<0) {
    enter_start_stop(score,snr,mouse_but);
    if (theViewStruct->scv_text->zoom->value) theViewStruct->zoomWin->PostMessage('zInv');
    return;
  }
  if (lnr>=min(sclin_max,sclin_max) || snr<0) return;
  if (mouse_but==B_SECONDARY_MOUSE_BUTTON) {  // right mouse button
    upd_endline(snr);  // may update score->len;
    return;
  }
  if (snr>=score->len) return;

  appWin->info_text->set_modif(0,true);
  cur_line=score->lin[lnr];
  ScSection *const sect=cur_line->sect+snr;
  ScSection *sec,
            *proto;
  key_info key;
  get_key_info(&key);
  if (debug) {
    for (int n=0;n<16;n++) { printf("%u ",key.key_states[n]); }
    putchar('\n');
  }
  int nr=key2num(&key),
      cat=sect->cat,
      ch=key2char(&key);
  if (nr<1) nr=1;
  prev_snr=snr;
  cur_lnr=lnr;
  cur_snr=snr;
  cur_sect=sect;
  if (ch) {
    appWin->act_action=ch;
    appWin->appView->mouseAction->set_active(ch);
  }
  else
    ch=appWin->act_action;
  bool to_higher=false,
       to_lower=false;
  switch (ch) {
    case 'up':
    case 'do':
      to_higher=make_higher[lnr%7];
      to_lower=make_lower[lnr%7];  // no break
    case 'ud':
      state=eToBeReset;
      for(sec=sect;sec->cat==ePlay;++snr,sec=cur_line->sect+snr) {
        bool stop = snr>=score->len-1 || sec->stacc || sec->sampled;
        switch (ch) {
          case 'up':
            if (to_higher) {
              sec->sign=0;
              *score->get_section(lnr-1,snr)=*sec;
              if (sec->sel) {
                selected.remove(lnr,snr);
                selected.insert(lnr-1,snr);
              }
              if (lnr>0) // maybe out-of-range
                sec->drawSect(this,snr,lnr-1);
              sec->reset();
            }
            else
              sec->sign=eHi;
            break;
          case 'do':
            if (to_lower) {
              sec->sign=0;
              *score->get_section(lnr+1,snr)=*sec;
              if (sec->sel) {
                selected.remove(lnr,snr);
                selected.insert(lnr+1,snr);
              }
              if (lnr<sclin_max)   // maybe out-of-range
                sec->drawSect(this,snr,lnr+1);
              sec->reset();
            }
            else
              sec->sign=eLo;
            break;
          case 'ud': sec->sign=0; break;
        }
        sec->drawSect(this,snr,lnr);
        if (stop) break;
      }
      return;
    case 'all':     // select all
    case 'scol':    // select all of 1 color
      if (selected.sv!=this) {
        selected.restore_sel();
        selected.sv=this;
      }
      state=eToBeReset;
      select_all(ch=='all');
      return;
    case 'uns':     // unselect
      if (selected.sv==this) {
        selected.restore_sel();
      }
      state=eToBeReset;
      return;
    case 'sel':     // select
      if (selected.sv!=this) {
        selected.restore_sel();
        selected.sv=this;
      }
      if (sect->cat==ePlay) {
        bool b=!sect->sel;
        for(sec=sect;sec->cat==ePlay;++snr,sec=cur_line->sect+snr) {
          if (sec->sel && !b)
            selected.remove(lnr,snr);
          else if (b)
            selected.insert(lnr,snr);  // all selected sections are stored
          sec->sel=b;
          sec->drawSect(this,snr,lnr);
          if (snr>=score->len-1 || sec->stacc || sec->sampled) return;
        }
      }
      else {
        state=eCollectSel;
        select_column(snr);
      }
      return;
    case 'move':
    case 'copy':
      if (selected.sv==this) {
        state= ch=='copy' ? eCopying :
               ch=='move' ? eMoving : 0;
        cur_point=prev_point=where;
        delta_lnr=delta_snr=0;
      }
      else
        appWin->appView->mouseAction->reset();
      return;
    case 'prta':
      if (sect->cat!=ePlay) {
        state=eToBeReset;
        return;
      }
      if (sect->port_dlnr) {
        sect->drawPortaLine(this,snr,lnr,true);
        sect->port_dlnr=sect->port_dsnr=0;
        state=eToBeReset;
      }
      else {
        be_app->SetCursor(square_cursor);
        cur_point=where;
        state=ePortaStart;
      }
      return;
  }
  if (nr==1) {  // MouseMoved will be enabled
    if (cat==ePlay) state=eErasing;
    else if (cat==eSilent) state=eTracking;
  }
  proto= snr>1 && (sect-1)->cat==ePlay && !(sect-1)->stacc && !(sect-1)->sampled ? sect-1 :
         snr<score->len-1 && (sect+1)->cat==ePlay && !(sect+1)->stacc && !(sect+1)->sampled ? sect+1 :
         0;
  for(sec=sect;nr>=1 && sec->cat==cat; --nr, ++snr, sec=cur_line->sect+snr) {
    switch (cat) {
      case ePlay:
        if (mouse_but==B_TERTIARY_MOUSE_BUTTON && !sec->sampled) // middle mouse button
          sec->stacc=!sec->stacc;
        else {
          if (sec->sel) selected.remove(lnr,snr);
          if (sec->port_dlnr) {
            sec->drawPortaLine(this,snr,lnr,true);
            sec->port_dlnr=sec->port_dsnr=0;
          }
          sec->cat=eSilent;
        }
        break;
      case eSilent:
        if (proto) {
          *sect=*proto;
          sec->sel=false;
          sec->port_dlnr=sec->port_dsnr=0;
        }
        else {
          sec->reset();
          sec->cat=ePlay;
          static bool &draw_col=appWin->draw_col->value;
          sec->note_col= draw_col ? appWin->act_color : score->ncol;
          sec->note_ampl=score->nampl;
          sec->sign=cur_line->note_sign;
          sec->sampled=appWin->use_raw->value;
        }
        sec->stacc= mouse_but==B_TERTIARY_MOUSE_BUTTON;
        break;
      default: alert("sect cat %d?",cat);
    }
    sec->drawSect(this,snr,lnr);
    if (snr>=score->len-1) return;
  }
}

void ScoreView::MouseMoved(BPoint where, uint32 location, const BMessage *) { 
  appWin->unfocus_textviews();
  if (!score || !cur_line) {
    state=eIdle; return;
  }
  switch (state) {
    case eTracking:
    case eErasing: {
        int snr,
            new_snr=sectnr(where.x);
        if (new_snr<=prev_snr) return;
        for (snr=prev_snr+1;;++snr) {
          if (snr<0 || snr>=score->len) { state=eIdle; return; }
          ScSection *const sect=cur_line->sect+snr;
          if (state==eTracking) {
            if (sect->cat==eSilent) {
              *sect=*cur_sect;
              sect->drawSect(this,snr,cur_lnr);
            }
            else if (sect->cat==ePlay) { state=eIdle; break; }
          }
          else {
            if (sect->cat==ePlay) {
              sect->cat=eSilent;
              sect->drawSect(this,snr,cur_lnr);
              if (sect->sel) selected.remove(cur_lnr,snr);
              if (sect->port_dlnr) {
                sect->drawPortaLine(this,snr,cur_lnr,true);
                sect->port_dlnr=sect->port_dsnr=0;
              }
              if (sect->stacc || sect->sampled) { state=eIdle; break; }
            }
            else if (sect->cat==eSilent) { state=eIdle; break; }
          }
          if (snr==new_snr) break;
        }
        prev_snr=new_snr;
      }
      break;
    case eMoving:
    case eMoving_hor:
    case eMoving_vert:
    case eCopying:
    case eCopying_hor:
    case eCopying_vert: {
        SLList_elem<SectData> *sd;
        int lnr1, snr1,
            dx=float2int((where.x-prev_point.x)/sect_len),
            dy=float2int((where.y-prev_point.y)/sclin_dist);
        if (dy || dx) {
          int x=abs(dx),
              y=abs(dy);
          if (state==eMoving) {
            if (x>y) state=eMoving_hor;
            else if (x<y) state=eMoving_vert;
            else break;
          }
          else if (state==eCopying) {
            if (x>y) state=eCopying_hor;
            else if (x<y) state=eCopying_vert;
            else break;
          }
          prev_point=where;
          for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // erase old ghost notes
            lnr1=sd->d.lnr + delta_lnr;
            snr1=sd->d.snr + delta_snr;
            if (lnr1>=0 && lnr1<sclin_max && snr1>=0 && snr1<score->len)
              score->get_section(lnr1,snr1)->drawS_ghost(this,snr1,lnr1,true);
          }
          int prev_delta_lnr=delta_lnr,
              prev_delta_snr=delta_snr;
          if (state==eMoving_vert || state==eCopying_vert)
            delta_lnr=float2int((where.y-cur_point.y)/sclin_dist);
          else if (state==eMoving_hor || state==eCopying_hor)
            delta_snr=float2int((where.x-cur_point.x)/sect_len);
          selected.check_direction(delta_lnr - prev_delta_lnr,delta_snr - prev_delta_snr);
          for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // draw new ghost notes
            lnr1=sd->d.lnr + delta_lnr;
            snr1=sd->d.snr + delta_snr;
            if (lnr1>=0 && lnr1<sclin_max && snr1>=0) {
              if (score->check_len(snr1+1)) redraw(false);
              score->get_section(lnr1,snr1)->drawS_ghost(this,snr1,lnr1,false);
            }
          }
        }
      }
      break;
    case eCollectSel: {
        int snr=sectnr(where.x);
        if (snr<0 || snr>=score->len) { state=eIdle; return; }
        if (snr>prev_snr)
          for (++prev_snr;;++prev_snr) {
            select_column(prev_snr);
            if (prev_snr==snr) break;
          }
        else if (snr<prev_snr)
          for (--prev_snr;;--prev_snr) {
            select_column(prev_snr);
            if (prev_snr==snr) break;
          }
      }
      break;
    case ePortaStart: {
        int snr=sectnr(where.x),
            lnr=linenr(where.y);
        ScSection *const sect=score->get_section(lnr,snr);
        if (sect->cat==ePlay || lnr==cur_lnr && snr==cur_snr)
          be_app->SetCursor(square_cursor);
        else
          be_app->SetCursor(point_cursor);
      }
      break;
    case eIdle:
    case eToBeReset:
      break;
    default:
      alert("mouse moving state %d?",state);
      state=eIdle;
  }
}

void ScoreView::MouseUp(BPoint where) {
  key_info key;
  get_key_info(&key);
  switch (state) {
    case eMoving_hor:
    case eMoving_vert:
    case eCopying_hor:
    case eCopying_vert: {
        SLList_elem<SectData> *sd,*lst_sd;
        ScSection *from, *to;
        int lnr1, snr1,
            to_lnr, to_snr;
        for (sd=selected.sd_list.lis;sd;sd=sd->nxt) { // erase ghost notes
          lnr1=sd->d.lnr + delta_lnr;
          snr1=sd->d.snr + delta_snr;
          if (lnr1>=0 && lnr1<sclin_max && snr1>=0 && snr1<score->len)
            score->get_section(lnr1,snr1)->drawS_ghost(this,snr1,lnr1,true);
        }
        if (key2char(&key)!='keep') {
          if (delta_lnr || delta_snr) {
            selected.check_direction(delta_lnr,delta_snr);
            for (lst_sd=0,sd=selected.sd_list.lis;sd;) {
              to_lnr=sd->d.lnr + delta_lnr;
              to_snr=sd->d.snr + delta_snr;
              from=score->get_section(sd->d.lnr,sd->d.snr);
              if (to_lnr>=0 && to_lnr<sclin_max && to_snr>=0) {
                if (score->check_len(to_snr+1)) redraw(false);
                to=score->get_section(to_lnr,to_snr);
                *to=*from;
                if (state==eMoving_vert || state==eCopying_vert) {  // sign is not copied
                  to->sign=score->lin[to_lnr]->note_sign;
                }
                if (state==eMoving_hor || state==eMoving_vert) {
                  if (from->port_dlnr) from->drawPortaLine(this,sd->d.snr,sd->d.lnr,true);
                  from->reset();
                }
                else
                  from->sel=false;
                from->drawSect(this,sd->d.snr,sd->d.lnr);
                sd->d.lnr=to_lnr;
                sd->d.snr=to_snr;
                if (to_lnr>=0 && to_lnr<sclin_max) {
                  to->drawSect(this,to_snr,to_lnr);
                  if (to->port_dlnr) to->drawPortaLine(this,to_snr,to_lnr,false);
                }
                lst_sd=sd; sd=sd->nxt;
              }
              else {
                if (state==eMoving_hor || state==eMoving_vert)
                  from->reset();
                else
                  from->sel=false;
                from->drawSect(this,sd->d.snr,sd->d.lnr);
                selected.remove(sd->d.lnr,sd->d.snr);
                sd=lst_sd ? lst_sd->nxt : selected.sd_list.lis;
              }
            }
          }
          if (state==eMoving_hor)
            score->drawEnd(this); // moving from nearby endline?
        }
      }
      appWin->appView->mouseAction->reset();
      break;
    case ePortaStart:
      appWin->appView->mouseAction->reset();
      be_app->SetCursor(B_HAND_CURSOR);
      { const int lnr=linenr(where.y),
                  snr=sectnr(where.x);
        ScSection *sec=score->get_section(lnr,snr);
        if (sec->cat==ePlay) {
          if (lnr-cur_lnr==0 && snr-cur_snr==0) { alert("portando: same note"); break; }
          if (cur_sect->note_col!=sec->note_col) { alert("portando notes not same color"); break; }
          if (abs(lnr-cur_lnr)>= 1<<5) { alert("portando notes height difference >= %d",1<<5); break; }
          if (snr-cur_snr<1) { alert("portando notes distance < 0"); break; }
          if (snr-cur_snr>= 1<<5) { alert("portando notes distance >= %d",1<<5); break; }
          cur_sect->port_dlnr= abs(lnr-cur_lnr);
          cur_sect->dlnr_sign= lnr>=cur_lnr;
          cur_sect->port_dsnr= snr-cur_snr-1;
          cur_sect->drawPortaLine(this,cur_snr,cur_lnr,false);
        }
      }
      break;
    case eCollectSel:
    case eIdle:
    case eTracking:
    case eErasing:
      break;
    case eMoving:
    case eCopying:
    case eToBeReset:
      appWin->appView->mouseAction->reset();
      break;
    default:
      alert("mouse-up state %d?",state);
  }
  state=eIdle;
}

ScoreViewStruct::ScoreViewStruct(BRect rect,int ident):
    id(ident),
    scroll_view(new ScoreSView(BRect(rect.left,rect.top,rect.left+scview_text-1,rect.bottom),this)),
    scv_view(scroll_view->scoreView),
    scv_text(new ScoreViewText(BRect(rect.left+scview_text,rect.top,rect.right,rect.bottom),this)),
    zoomWin(0) {
}

void ScoreViewBase::enter_start_stop(Score *score,int snr,uint32 mouse_but) {
  switch (mouse_but) {
    case B_PRIMARY_MOUSE_BUTTON:
      if (play_start==snr) {
        draw_start_stop(score,play_start,0,true);
        play_start=0;
      }
      else {
        draw_start_stop(score,play_start,snr,true);
        play_start=snr;
        if (play_stop>=0 && play_stop<play_start) alert("stop < start");
      }
      break;
    case B_SECONDARY_MOUSE_BUTTON:
      if (play_stop==snr) {
        draw_start_stop(score,play_stop,0,false);
        play_stop=-1;
      }
      else {
        draw_start_stop(score,play_stop,snr,false);
        play_stop=snr;
        if (play_start && play_stop<play_start) alert("stop < start");
      }
      break;
    case B_TERTIARY_MOUSE_BUTTON:
      if (play_stop>=0) {
        draw_start_stop(score,play_stop,0,false);
        play_stop=-1;
      }
      if (play_start>0) {
        draw_start_stop(score,play_start,0,true);
        play_start=0;
      }
      break;
  }
}

void MusicView::MouseDown(BPoint where) {
  uint32 mouse_but;
  GetMouse(&where,&mouse_but);
  int snr=sectnr(where.x);
//  key_info key; get_key_info(&key); int ch=key2char(&key);
  enter_start_stop(score,snr,mouse_but);
}

bool exec_info(int snr,Score *sc) {
  if (appWin->no_set->value) return true;
  ScInfo* sci=sc->scInfo;
  if (!sci) return true;
  for (sci=sci+snr;sci;sci=sci->next) {
    if (sci->tag && !appWin->exec_cmd(*sci)) return false;
  }
  return true;
}

struct AmcApp: public BApplication {
  char *appf,  // argv[0]
       *inf;   // .sco or .scr file
  AmcApp(char *app,char *in): BApplication(app_mime),appf(app),inf(in) {
    appFont=new BFont(be_plain_font);
    appFont->SetSize(12);

    bitmaps=new Bitmaps();
    square_cursor=new BCursor(sq_cursor_data);
    point_cursor=new BCursor(pnt_cursor_data);
  }
  void ReadyToRun() {
    new AppWindow(appWin,appf,inf);
    appWin->Show();
  }
/*
  void ArgvReceived(int32 argc,char **argv) {
  }
*/
  void RefsReceived(BMessage *msg) {
    if (debug) alert("refs received!");
    if (!IsLaunching()) return;

    uint32 type;
    entry_ref ref;
    static BPath path;

    msg->GetInfo("refs", &type);
    if (type != B_REF_TYPE) return;
    if (msg->FindRef("refs", &ref) == B_OK) {
      BEntry entry(&ref);
      path.SetTo(&entry);
      inf=const_cast<char*>(path.Path());
    }
  }
  void MessageReceived(BMessage* mes) {
    switch (mes->what) {
      case ALIVE:     // from amc-scheme
        break;
      case STORE_SEL: // from amc-scheme
      case SEND_NOTES:
        appWin->PostMessage(mes);
        break;
      default:
        BApplication::MessageReceived(mes);
    }
  }
};

int main(int argc,char **argv) {
  char *inf=0;
  for (int an=1;an<argc;++an) {
    if (!strcmp(argv[an],"-h")) {
      puts("Usage:");
      puts("amc [file.sco | file.scr]");
      exit(1);
    }
    if (!strcmp(argv[an],"-db")) debug=true;
    else if (argv[an][0]=='-') { alert("Unexpected option %s\n",argv[an]); exit(1); }
    else inf=argv[an];
  }
  // fputs("\033[34m",stdout); // terminal: blue
  AmcApp app(argv[0],inf);
  app.Run();
  // fputs("\033[30m",stdout); // black
  return 0;
} 
