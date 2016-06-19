#include <stdio.h>
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <TextView.h>

#include "str.h"
#include "widgets.h"
#include "amc.h"
#include "ndist.h"

const int x_off=4,
          y_off=6,
          sect_len=6,
          sclin_dist=4;

extern rgb_color cBlack,cGrey;

struct NoteDistView: BView {
  int start,
      stop;
  Score *score;
  NoteDistView(BRect rect);
  void drawSection(ScSection *sect,int lnr,int snr);
  void Draw(BRect);
};

NoteDistView::NoteDistView(BRect rect):
    BView(rect,0,B_FOLLOW_ALL,B_WILL_DRAW) {
  AddChild(new Button(BRect(2,Bounds().bottom-20,0,0),"close",'ndiC'));
}

const float win_height=halftones_max*sclin_dist+2*y_off+20;

void NoteDistWindow::set(int n1,int n2,Score *sc) {
  ResizeTo(2*x_off+(n2-n1+1)*sect_len,win_height);
  view->start=n1; view->stop=n2; view->score=sc;
}

NoteDistWindow::NoteDistWindow(BPoint top):
    BWindow(BRect(top.x,top.y,top.x+100,top.y+win_height),0,
            B_MODAL_WINDOW_LOOK,B_NORMAL_WINDOW_FEEL,B_NOT_RESIZABLE),
    view(new NoteDistView(Bounds())) {
  AddChild(view);
}

void NoteDistView::drawSection(ScSection *sect,int lnr,int n) {
  int act_lnr=score->to_half(lnr,sect->sign);
  float x=x_off+n*sect_len,
        y=y_off+act_lnr*sclin_dist;
  SetHighColor(color(sect->note_col));
  SetPenSize(3);
  int slen= sect->stacc ? sect_len-3 : sect_len-1;
  StrokeLine(BPoint(x,y),BPoint(x+slen,y),B_SOLID_HIGH);
}

void NoteDistView::Draw(BRect) {
  int n,
      lnr,snr;
  ScSection *sect;
  SetPenSize(1);
  float line_len=(stop-start+1)*sect_len;
  for (n=0;n<=halftones_max;++n) {
    if ((n+1)%12==0) SetHighColor(cBlack);
    else SetHighColor(cGrey);
    float y=y_off + n * sclin_dist;
    StrokeLine(BPoint(x_off,y),BPoint(x_off+line_len,y),B_SOLID_HIGH);
  }
  for (lnr=0;lnr<sclin_max;++lnr) {
    for (snr=start;snr<=stop;++snr) {
      sect=score->get_section(lnr,snr);
      if (sect->cat==ePlay)
        drawSection(sect,lnr,snr - start);
    }
  }
}

void NoteDistWindow::MessageReceived(BMessage *mes) {
  switch (mes->what) {
    case 'ndiC': // close window
      appWin->PostMessage(mes);
      break;
    default:
      BWindow::MessageReceived(mes);
   } 
}
