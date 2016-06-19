#include <stdio.h>
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <TextView.h>
#include "str.h"
#include "widgets.h"
#include "amc.h"
#include "chords.h"

const int keys_max=13,    // key buttons
          chords_max=15;  // chord buttons
const float rbut_dist=14; // radio button distance

extern rgb_color cBlack,
                 cForeground,
                 cBackground;

struct ChordsView:BView {
  ChordsWindow *win;
  BRect toneNr_area;
  ChordsView(BRect rect,ChordsWindow *win);
  void Draw(BRect);
  void draw_tone_numbers();
};

ChordsView::ChordsView(BRect rect,ChordsWindow *w):
    BView(rect,0,B_FOLLOW_NONE,B_WILL_DRAW),
    win(w) {
  SetViewColor(cForeground);
  SetLowColor(cForeground);
  SetFont(appFont,B_FONT_SIZE);
  BPoint top(2,0);
  RadioButton *rbut;
  static char
    *majkeys[keys_max] = {
      "B","Bes","A","As","G","Ges","Fis","F","E","Es","D","Des","C"
    },
    *minkeys[keys_max] = {
      "gis","g","fis","f","e","es","dis","d","cis","c","b","bes","a"
    },
    *nr_signs[keys_max] = {
      "(5 #)","(2 b)","(3 #)","(4 b)","(1 #)","(6 b)",
      "(6 #)","(1 b)","(4 #)","(3 b)","(2 #)","(5 b)",0
    },
    *chords[chords_max] = {
      "","m","o","+","maj7","7","m7","/o7","o7","9","-9","9(maj7)","m9","+9","(add6)"
    };
  int n;
  ChordsWindow::ToneData *td;
  for (n=0;;++n) {
    int ind=keys_max-1-n;
    rbut=win->tone->AddButton(this,top,"",ind);
    td=win->toneData+ind;
    td->y=top.y+rbut->dy();
    td->majkey=majkeys[n];
    td->minkey=minkeys[n];
    td->nr_signs=nr_signs[n];
    top.y+=rbut_dist;
    if (n==keys_max-1) {
      rbut->SetValue(true);
      toneNr_area.Set(110,0,120,top.y);
      break;
    }
  }
  top.Set(135,0);
  for (n=0;n<chords_max;++n) {
    rbut=win->chord->AddButton(this,top,chords[n],n);
    top.y+=rbut_dist;
  }
}

void ChordsView::Draw(BRect) {
  int n,n1,n2,val;
  const int chord_note_max=20;
  const int ndist=100/chord_note_max;
  ChordsWindow::ToneData *td;

  SetHighColor(cBlack); // tone labels: major, minor, signs
  for (n=0;n<keys_max;++n) { 
    td=win->toneData+n;
    DrawString(td->majkey,BPoint(20,td->y));
    DrawString(td->minkey,BPoint(50,td->y));
    if (td->nr_signs)
      DrawString(td->nr_signs,BPoint(75,td->y));
  }
  draw_tone_numbers(); // updateable tone sequence nr

  SetHighColor(cBackground); // separation line
  SetPenSize(2);
  StrokeLine(BPoint(125,0),BPoint(125,Bounds().bottom));

  SetHighColor(cBlack); // chord diagrams
  SetPenSize(1);
  BPoint pnt1(200,7);
  for (n=0;n<chords_max;++n) {
    win->assign_notes(n);
    for (n1=0;n1<chord_note_max;++n1) {
      float x=pnt1.x+n1*ndist;
      for (n2=0;;++n2) {
        val=win->chord_notes[n2];
        if (val==nop) {
          StrokeLine(BPoint(x,pnt1.y-1),BPoint(x,pnt1.y+1));
          break;
        }
        if (val==n1) {
          StrokeLine(BPoint(x,pnt1.y-3),BPoint(x,pnt1.y+3));
          if (win->chord_notes[n2+1]==nop) goto done;
          break;
        }
      }
    }
    done:
    StrokeLine(pnt1,BPoint(pnt1.x + n1 * ndist,pnt1.y));
    pnt1.y+=rbut_dist;
  }
}

void ChordsWindow::assign_notes(int chord_nr) {
  int n=1;
  Array<int,10>&cn=chord_notes;
  cn[0]=0;   // usually lowest note = base note
  switch (chord_nr) {
    case 0:
      cn[n++]=4; cn[n++]=7; break;
    case 1:  // m
      cn[n++]=3; cn[n++]=7; break;
    case 2:  // o
      cn[n++]=3; cn[n++]=6; break;
    case 3:  // +
      cn[n++]=4; cn[n++]=8; break;
    case 4:  // maj7
      cn[n++]=4; cn[n++]=7; cn[n++]=11; break;
    case 5:  // 7
      cn[n++]=4; cn[n++]=7; cn[n++]=10; break;
    case 6:  // m7
      cn[n++]=3; cn[n++]=7; cn[n++]=10; break;
    case 7:  // /o7
      cn[n++]=3; cn[n++]=6; cn[n++]=10; break;
    case 8:  // o7
      cn[n++]=3; cn[n++]=6; cn[n++]=9; break;
    case 9:  // 9
      cn[n++]=4; cn[n++]=7; cn[n++]=10; cn[n++]=14; break;
    case 10:  // -9
      cn[n++]=4; cn[n++]=7; cn[n++]=10; cn[n++]=13; break;
    case 11:  // 9(maj7)
      cn[n++]=4; cn[n++]=7; cn[n++]=11; cn[n++]=14; break;
    case 12:  // m9
      cn[n++]=3; cn[n++]=7; cn[n++]=10; cn[n++]=14; break;
    case 13:  // +9
      cn[n++]=4; cn[n++]=7; cn[n++]=10; cn[n++]=15; break;
    case 14:  // (add6)
      cn[n++]=4; cn[n++]=7; cn[n++]=9; break;
    default: alert("chord %d?",chord_nr);
  }
  cn[n]=nop;
}

ChordsWindow::ChordsWindow(BPoint top):
    BWindow(BRect(top.x,top.y,top.x+280,top.y+230),"Chords",B_MODAL_WINDOW_LOOK,B_NORMAL_WINDOW_FEEL,
            B_NOT_RESIZABLE|B_ASYNCHRONOUS_CONTROLS),
    tone(new RButtonArrowCtrl('base')),
    chord(new RButtonArrowCtrl('chor')),
    the_key_nr(0),
    the_chord(0),
    the_distance(0),
    toneData(new ToneData[keys_max]),
    view(new ChordsView(Bounds(),this)) {
  Button *but;
  AddChild(view);
  float x=2;
  but=new Button(BRect(x,Bounds().bottom-40,0,0),"close",'chdC',B_FOLLOW_NONE);
  view->AddChild(but);
  but=new Button(BRect(x,Bounds().bottom-20,0,0),"set key",'setk',B_FOLLOW_NONE);
  view->AddChild(but);
  x+=but->dx()+2;
  but=new Button(BRect(x,Bounds().bottom-20,0,0),"force key",'frck',B_FOLLOW_NONE);
  view->AddChild(but);
}

void ChordsView::draw_tone_numbers() {
  int n;
  static char *nrs[keys_max] = { 0,"1","2","3","4","5","6","7" };
  static int toneNrs[][keys_max] = {
    // C   D   E F     G   A   B  <-- C major
     { 1,0,2,0,3,4,0,0,5,0,6,0,7 }, // C
     { 7,1,0,2,0,3,4,4,0,5,0,6,0 }, // Des
     { 0,7,1,0,2,0,3,3,0,4,5,0,6 }, // D
     { 6,0,7,1,0,2,0,0,3,4,0,5,0 }, // Es
     { 0,6,0,7,1,0,2,2,0,3,4,0,5 }, // E
     { 5,0,6,0,7,1,0,0,2,0,3,4,0 }, // F
     { 0,5,0,6,0,7,1,1,0,2,0,3,4 }, // Fis
     { 0,5,0,6,0,7,1,1,0,2,0,3,4 }, // Ges
     { 4,0,5,0,6,0,7,7,1,0,2,0,3 }, // G
     { 3,4,0,5,0,6,0,0,7,1,0,2,0 }, // As
     { 0,3,4,0,5,0,6,6,0,7,1,0,2 }, // A
     { 2,0,3,4,0,5,0,0,6,0,7,1,0 }, // Bes
     { 0,2,0,3,4,0,5,5,0,6,0,7,1 }, // B
  };
  SetHighColor(cForeground);  // erase old numbers
  FillRect(toneNr_area);
  SetHighColor(cBlack);
  int *act_tone_nrs=toneNrs[win->the_key_nr];
  for (n=0;n<keys_max;++n)
    if (act_tone_nrs[n])
      DrawString(
        nrs[act_tone_nrs[n]],
        BPoint(toneNr_area.left,toneNr_area.top+win->toneData[n].y)
      );
}

void ChordsWindow::MessageReceived(BMessage* mes) {
  switch (mes->what) {
    case 'base': { // from tone radiobutton
        int distance[13]={ 0,1,2,3,4,5,6,6,7,8,9,10,11 };
        mes->FindInt32("",&the_key_nr);
        the_distance=distance[the_key_nr];
        view->draw_tone_numbers();
      }
      break;
    case 'chor': { // from chord radiobutton
        mes->FindInt32("",&the_chord);
        assign_notes(the_chord);
        appWin->PostMessage('chkn');
      }
      break;
    case 'setk': // from set key button
    case 'frck': // from force key button
    case 'chdC': // close window
      appWin->PostMessage(mes);
      break;
    default:
      BWindow::MessageReceived(mes);
   }
}
