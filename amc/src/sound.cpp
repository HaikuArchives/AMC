#include <stdio.h>
#include <stdlib.h>
#include <Application.h>
#include <SoundPlayer.h>
#include <Window.h>
//#include <Button.h>
#include <TextView.h>

#include "str.h"
#include "widgets.h"
#include "dump_wav.h"
#include "colors.h"
#include "amc.h"
#include "midi-out.h"
#include "sound.h"
#include "read-wav-data.h"
#include "midi-keyb.h"

KeybTune keyboard_tune;

void KeybTune::reset() { cur_ind=-1; turn_start=0; }
void KeybTune::nxt_turn() { turn_start+=tune_wid; }
KeybTune::KeybTune() { reset(); }
void KeybTune::add(int lnr,int sign,int turn_s,int snr1,int snr2) {
  if (cur_ind>=1000-1) return;  // no warning
  ++cur_ind;
  KeybNote *kn=buf+cur_ind;
  kn->lnr=lnr;
  kn->sign=sign;
  kn->snr=snr1+turn_s;
  kn->dur=snr2-snr1;
  if (kn->dur<0) kn->dur+=tune_wid; // note coinceeds with repeat
  if (kn->dur==0) kn->dur=1;  // notes may have been too short
}

const int
  sshift=10,
  sbm=1<<sshift,    // less: no clean low tones
  smask=sbm-1,
  at_scale=2,       // attack scaling
  ad_max=4,         // attack and decay sounds
  noise_bufs_max=5, // buffers in iGreen
  tscale=40000/subdiv, // tempo scaling
  voice_max=10,
  mk_voice_max=5,   // midi keyboard voices
  stereo_delay=40;  // delay = stereo_delay * 1/22KHz = 1.8 ms

enum {
  eSleep=1, ePause, eNote, ePortaNote, eSampled, // note types
  eLeft, eRight  // stereo location
};

int Fsize,     // = 4096
    IBsize,    // instrument buffer (= 1024)
    scope_win; // = 128, display in oscilloscope

const float freq_scale=sbm/50000.;

int *tmp_buf_r, *tmp_buf_l, // used by bufferProc()
    ind_instr_buf;  // index for instrument buffers

struct {   // set by playScore(), used by bufferProc()
  int snr,        // current section nr
      snr2,       // idem, less exact, used for sync with keyboard and display
      snr_start,
      tcount;
} cur;

int16 *wave_buf;    // used for wave output

struct Sinus {
  int buf[sbm];
  Sinus() {
    int i,f;
    for (i=0;i<sbm/2;++i) {
      f=i<<3;
      buf[i]=f-(f*f>>(sshift+2));
      buf[i+sbm/2]=-buf[i];
    }
  }
} sinus;

inline int interpol(int *buf,int ind) {
  return buf[ind]*(sbm-ind) + buf[ind+1]*ind >> sshift;
}

struct Pulse {
  int buf[sbm],
      buf3[sbm];
  Pulse() {
    const int slope=32;
    int n,
        i1=sbm/slope,
        i2=4*i1,
        i3=12*i1;
    buf[0]=0;
    for (n=1;n<i1;++n) buf[n]=buf[n-1]+slope;
    for (;n<i2;++n) buf[n]=sbm;
    for (;n<i3;++n) buf[n]=buf[n-1]-slope/8;
    for (;n<sbm;++n) buf[n]=0;
    for (n=0;n<sbm;++n) buf3[n]=(buf[n]+buf[2*n%sbm]-buf[3*n%sbm])/2;
    // for (n=0;n<sbm;++n) buf3[n]=(buf[n]+buf[3*n%sbm])/2;
  }
} pulse;


struct Sinus_with_harmonics {  // sinus with harmonics
  int buf[sbm],
      mult_ampl[harm_max],
      *mult_freq;
  Sinus_with_harmonics() {
    static int mf[]={ 1,2,3,6,10 };
    mult_freq=mf;
  }
  void recalc() {
    int i,harm;
    for (i=0;i<sbm;++i) buf[i]=0;
    for (harm=0;harm<harm_max;++harm)
      for (i=0;i<sbm;++i)
        if (mult_ampl[harm])
          buf[i]+=sinus.buf[i*mult_freq[harm] & smask] * mult_ampl[harm] >> sshift;
  }
  void set_a(int *harmon) {
    static int scale_ampl[]={ 0,sbm/4,sbm/2,sbm };
    for (int i=0;i<harm_max;++i) mult_ampl[i]=scale_ampl[harmon[i]]; // harmon[i]: values 0 - 3
    recalc();
  }
};

struct Sawtooth {
  int buf[sbm];
    Sawtooth() {
    int i,
        i1=sbm/10,
        i2=sbm-i1,
        dy;
    dy=sbm/i1;
    buf[0]=0;
    for (i=1;i<=i1;i++) buf[i]=buf[i-1]+dy;
    dy=2*sbm/(i2-i1-1);
    for (;i<i2;i++) buf[i]=buf[i-1]-dy;
    dy=sbm/(sbm-i2);
    for (;i<sbm;i++) buf[i]=buf[i-1]+dy;
  }
} sawt;

struct VarSinus {
  int buf[sbm];
  VarSinus(int formant,int nrsin) { // formant: 1,2,4,7 nrsin: 1-3
    for (int i=0;i<sbm;i++)
      buf[i]=sinus.buf[nrsin*8*i*sbm/(formant*sbm+(8-formant)*i) & smask];
  }
};

struct Sound {
  int *wave,
      ampl;
  Sound() { }
  Sound(int* w,int a):wave(w),ampl(a) { }
};

struct Instrument {
  int attack,   // attack or start duration
      decay,    // decay duration
      *s_loc,   // stereo location: eLeft,0,eRight
      stereo_mid, // 0
      d_ind;
  int16 d_buf[stereo_delay], // stereo delay buffer
        *buf;   // temporary buf
  Sound *sbuf,  // steady-state tone
        *decay_data,
        *attack_data;
  Instrument(int att,int dec):
      attack(att),decay(dec),s_loc(&stereo_mid),stereo_mid(0),
      buf(0),sbuf(0),decay_data(0),attack_data(0) {
  }
  virtual void nxt_val(float,float&,float&) { }
  virtual Instrument* get_act_instr() { return this; }
  void set_decay(int n) {
    static int scale[]={ 1,2,5,10,20,40 }; // decay value is 0 - 5
    decay=scale[n];
  }
  void set_attack(int n) {
    static int scale[]={ 0,1,2,5,10,20 }; // attack value is 0 - 5
    attack=scale[n];
  }
  void set_startup(int n) {
    static int scale[]={ 0,1,2,5,10,20 }; // attack value is 0 - 5
    attack=scale[n];
  }
  virtual void init(int dim) { buf=new int16[dim]; }
  int delay(int val) {
    int ind=(d_ind+1)%stereo_delay;
    int out=d_buf[ind];
    d_ind=(++d_ind)%stereo_delay;
    d_buf[d_ind]=val;
    return out;
  }
};

struct Note {
  int8 cat,
       note_col,
       ampl,
       act_lnr; // line nr;
  Instrument *act_instr;
  int dur,
      attack,
      decay,
      remain,  // remaining time, will be re-assigned
      act_snr; // section nr
  float freq;
  void set_timing(int t) { dur=remain=t; }
};

Note* NOTES[voice_max];

struct PtrToNote {
  int ev_time;
  Note *not;
  uint8 midi_tag; // 0x80: stop note, 0x90: start note
  PtrToNote(Note *n,int ev,uint8 tag):ev_time(ev),not(n),midi_tag(tag) { }
  bool operator<(PtrToNote &other) { return ev_time < other.ev_time; }
  bool operator==(PtrToNote &other) { return false; }   // always insert
};

SLinkedList<PtrToNote> midi_events;

struct MidiNoteBuf {
  float ind_f,ind_f2;
  int lnr,sign,turn_s,start_snr,
      occ;      // 2: occupied, 1: decaying
  float freq;
  Instrument *instr;
  MidiNoteBuf() { reset(); }
  void reset();
};

struct NoteBuffer {
  float ind_f,ind_f2;
  int busy,      // note duration + decay
      len,
      cur_note,
      mode;      // used by instrument
  Note *notes;
  Instrument *instr;
  NoteBuffer();
  void reset();
  void renew();
  void report(int);
};

int ampl2ampl(int ampl,int val) {  // ampl between 1 and 6
  static int mult[]={ 0,12,20,30,50,80,128 };
  return mult[ampl] * val >> 7;
}

struct Lfo {
  float ind_f,
        freq;
  int* buf;
  Lfo():ind_f(0),freq(0.4),buf(0) { }
  void update() {
    int n,
        ind;
    for (n=0;n<IBsize;++n) {
      ind_f += freq;
      ind=static_cast<int>(ind_f);
      if (ind >= sbm) { ind-=sbm; ind_f=ind; }
      buf[n]=ind;
    }
  }
};

struct iRed: Instrument {
  VarSinus *var_sinus[5][3];
  int *at,
      *b,
      at1,b1,b2,
      s_a[3];
  void set(int x,int y,int formant,int nr) {
    var_sinus[x][y]=new VarSinus(formant,nr);
  }
  iRed():Instrument(3,2) {
    int n;
    set(0,0,7,2); set(0,1,7,3); set(0,2,7,5);
    set(1,0,5,2); set(1,1,5,3); set(1,2,5,5);
    set(2,0,3,2); set(2,1,3,3); set(2,2,3,5);
    set(3,0,2,2); set(3,1,2,3); set(3,2,2,5);
    set(4,0,1,2); set(4,1,1,3); set(4,2,1,5);
    s_a[0]=s_a[1]=s_a[2]=sbm;
    at=var_sinus[3][1]->buf;
    b=var_sinus[2][1]->buf;
    sbuf=new Sound(&b1,sbm);
    static Sound sd[]={ Sound(&at1,sbm),Sound(&at1,sbm),Sound(&b2,sbm),Sound(&b1,sbm) };
    attack_data=sd;
    static Sound dd[]={ Sound(&b1,sbm),Sound(&b1,sbm*3/5),Sound(&b1,sbm*2/5),Sound(&b1,sbm/5) };
    decay_data=dd;
  }
  void nxt_val(float freq,float &ind_f,float &) {
    ind_f += freq;
    int ind=static_cast<int>(ind_f);
    if (ind >= sbm) { ind-=sbm; ind_f=ind; }
    at1=at[ind];
    b1=b[ind];
    b2=(at1+b1)/2;
  }
  void set_start_timbre(int f,int n) {
    at=var_sinus[f][n]->buf;
  }
  void set_timbre(int f,int n) {
    b=var_sinus[f][n]->buf;
  }
  void set_start_ampl(int n) {  // n from 0 to 3
    static int sa[4][3] = { { 0,sbm/2,sbm*2/3 },{ sbm/2,sbm*2/3,sbm*3/4 },
                            { sbm,sbm,sbm },{ sbm*3/2,sbm*3/2,sbm*4/3 } };
    for (int i=0;i<3;++i) attack_data[i].ampl=sa[n][i];
  }
} ired;

struct iOrange: Instrument {
  int *b,
      b1;
  iOrange():Instrument(0,1) {
    int n;
    b=ired.var_sinus[2][1]->buf;
    sbuf=new Sound(&b1,sbm);
    static Sound ad[]={ Sound(&b1,sbm/8),Sound(&b1,sbm/2),Sound(&b1,sbm*3/4),Sound(&b1,sbm) };
    attack_data=ad;
    static Sound dd[]={ Sound(&b1,sbm),Sound(&b1,sbm*4/5),Sound(&b1,sbm*2/5),Sound(&b1,sbm/5) };
    decay_data=dd;
  }
  void nxt_val(float freq,float &ind_f,float &) {
    ind_f += freq;
    int ind=static_cast<int>(ind_f);
    if (ind >= sbm) { ind-=sbm; ind_f=ind; }
    b1=b[ind];
  }
  void set_timbre(int f,int n) {
    b=ired.var_sinus[f][n]->buf;
  }
} iorange;

struct iBlack: Instrument {
  float fm_freq,
        fm_mod;
  int b;
  iBlack():Instrument(0,1),fm_freq(0.4),fm_mod(0.01) {
    static Sound ad[]={ Sound(&b,sbm/8),Sound(&b,sbm/2),Sound(&b,sbm*3/4),Sound(&b,sbm) };
    attack_data=ad;
    sbuf=new Sound(&b,sbm);
    static Sound dd[]={ Sound(&b,sbm),Sound(&b,sbm*7/10),Sound(&b,sbm*4/10),Sound(&b,sbm*2/10) };
    decay_data=dd;
  }
  void nxt_val(float freq,float &ind_f,float &ind_f2) {
    int fm,ind_fm,ind;
    ind_f2 += freq*fm_freq;
    ind_fm=static_cast<int>(ind_f2);
    if (ind_fm >= sbm) { ind_fm=ind_fm & smask; ind_f2=ind_fm; }
    fm=sinus.buf[ind_fm];
    ind_f += freq*(1+fm*fm_mod);
    ind=static_cast<int>(ind_f);
    if (ind>=sbm) { ind=ind & smask; ind_f=ind; }
    else if (ind<0) { ind=sbm-(-ind & smask); ind_f=ind; }
    b=sinus.buf[ind];
    // b=ired.var_sinus[1][1]->buf[ind]; <-- not good
  }
} iblack;

struct iBrown: Instrument {
  float fm_freq,
        fm_mod,
        detun;
  int b;
  iBrown():Instrument(0,1),fm_freq(0.4),fm_mod(0.01) {
    static Sound ad[]={ Sound(&b,sbm/8),Sound(&b,sbm/2),Sound(&b,sbm*3/4),Sound(&b,sbm) };
    attack_data=ad;
    sbuf=new Sound(&b,sbm);
    static Sound dd[]={ Sound(&b,sbm),Sound(&b,sbm*7/10),Sound(&b,sbm*4/10),Sound(&b,sbm*2/10) };
    decay_data=dd;
  }
  void nxt_val(float freq,float &ind_f,float &ind_f2) {
    int fm,ind_fm,ind;
    ind_f2 += (freq+detun)*fm_freq;
    ind_fm=static_cast<int>(ind_f2);
    if (ind_fm >= sbm) { ind_fm=ind_fm & smask; ind_f2=ind_fm; }
    fm=sinus.buf[ind_fm];
    ind_f += freq*(1+fm*fm_mod);
    ind=static_cast<int>(ind_f);
    if (ind>=sbm) { ind=ind & smask; ind_f=ind; }
    else if (ind<0) { ind=sbm-(-ind & smask); ind_f=ind; }
    b=sinus.buf[ind];
  }
  void set_detune(int n) {
    static float scale[]={ 0,0.02,0.05,0.1,0.2,0.4 }; // detune value 0 - 5
    detun=scale[n];
  }
} ibrown;

const float
  freq_arr[8]  = { 1, 2, 3, 4, 5, 6, 7, 8 },
  subband_arr[8]= { 0.5, 0.667, 0.75, 1.25, 1.33, 1.4, 1.67, 2.67 },
  index_arr[8] = { 0, 0.5, 1, 2, 3, 5, 7, 10 };

void BlackCtrl::set_fm(int m) {
  float index;
  static bool& subb=sub_band->value;
  switch (m) {
    case 1:
      iblack.fm_freq= subb ? subband_arr[fm_ctrl->value] : // value: 0 - 7
                             freq_arr[fm_ctrl->value];
      act_freq=iblack.fm_freq;
      break;
    case 2:
      index=index_arr[fm_ctrl->valueY];  // value: 0 - 7
      display_mod=index;
      iblack.fm_mod=index/sbm;
      break;
  }
}

void BrownCtrl::set_fm(int m) {
  float index;
  static bool& subb=sub_band->value;
  switch (m) {
    case 1:
      ibrown.fm_freq= subb ? subband_arr[fm_ctrl->value] : // value: 0 - 7
                             freq_arr[fm_ctrl->value];
      act_freq=ibrown.fm_freq;
      break;
    case 2:
      index=index_arr[fm_ctrl->valueY];  // value: 0 - 7
      display_mod=index;
      ibrown.fm_mod=index/sbm;
      break;
  }
}

struct iPurple: Instrument {
  Sinus_with_harmonics st_hsinus, // for startup
                       hsinus;    // for steady state
  int *at,
      *b,
      at1,b1;
  iPurple():Instrument(2,2) {
    at=st_hsinus.buf;
    b=hsinus.buf;
    sbuf=new Sound(&b1,sbm/2);
    static Sound sd[]={ Sound(&at1,sbm/4),Sound(&at1,sbm/2),Sound(&at1,sbm/2),Sound(&b1,sbm/2) };
    attack_data=sd;
    static Sound dd[]={ Sound(&b1,sbm/2),Sound(&b1,sbm*3/10),Sound(&b1,sbm*2/10),Sound(&b1,sbm/10) };
    decay_data=dd;
  }
  void nxt_val(float freq,float &ind_f,float &) {
    ind_f += freq;
    int ind=static_cast<int>(ind_f);
    // float f=ind_f; int ind=static_cast<int>(f/2+f*f/2/sbm);  <-- asymmetric wave
    if (ind >= sbm) { ind-=sbm; ind_f=ind; }
    at1=at[ind]; b1=b[ind];
  }
} ipurple;

void PurpleCtrl::set_st_hs_ampl(int *ah) { ipurple.st_hsinus.set_a(ah); }

void PurpleCtrl::set_hs_ampl(int *h) { ipurple.hsinus.set_a(h); }

struct iBlue: Instrument {
  int b;
  void set_att_mode(bool piano) {
    if (piano) {
      static Sound ad[]={ Sound(&b,sbm),Sound(&b,sbm),Sound(&b,sbm/2),Sound(&b,0) };
      attack_data=ad;
      decay_data=0;
      decay=0;
    }
    else {
      static Sound ad[]={ Sound(&b,sbm/4),Sound(&b,sbm/2),Sound(&b,sbm),Sound(&b,sbm) };
      attack_data=ad;
      static Sound dd[]={ Sound(&b,sbm),Sound(&b,sbm*7/10),Sound(&b,sbm*3/10),Sound(&b,sbm/10) };
      decay_data=dd;
    }
  }
  iBlue():Instrument(0,4) {
    sbuf=new Sound(&b,sbm);  // attack can be 0
    set_att_mode(false);
  }
  void set_p_attack(int n) {
    static int scale[]={ 0,5,10,20,50,100 }; // n is 0 - 5
    attack=scale[n];
  }
  void nxt_val(float freq,float &ind_f,float &ind_f2) {
    int ind;
    const float diff=freq_scale/2;
    static bool &chorus=appWin->blue_control->chorus->value,
                &rich=appWin->blue_control->rich->value;
    ind_f += freq;
    ind=static_cast<int>(ind_f);
    if (ind >= sbm) { ind-=sbm; ind_f=ind; }
    if (chorus) {
      ind_f2 -= freq + diff;
      int ind2=static_cast<int>(ind_f2);
      if (ind2 < 0) { ind2 += sbm; ind_f2=ind2; }
      if (rich) b=pulse.buf3[ind] - pulse.buf3[ind2];
      else b=pulse.buf[ind] - pulse.buf[ind2];
    }
    else if (rich)
      b=pulse.buf3[ind];
    else
      b=pulse.buf[ind];
  }
} iblue;

void BlueCtrl::set_attack() {
  iblue.set_attack(attack->value);
}

void BlueCtrl::set_decay() {
  if (p_attack->value) iblue.set_p_attack(max(1,decay->value));
  else iblue.set_decay(decay->value);
  iblue.set_att_mode(p_attack->value);
}

void BlueCtrl::set_piano() {
  if (p_attack->value) iblue.set_p_attack(max(1,decay->value));
  else { iblue.set_decay(decay->value); iblue.set_attack(attack->value); }
  iblue.set_att_mode(p_attack->value);
}

void BrownCtrl::set_detune() {
  ibrown.set_detune(detune->value);
}

void BlackCtrl::set_attack() {
  iblack.set_attack(attack->value);
}

void BlackCtrl::set_decay() {
  iblack.set_decay(decay->value);
}

void BrownCtrl::set_attack() {
  ibrown.set_attack(attack->value);
}

void BrownCtrl::set_decay() {
  ibrown.set_decay(decay->value);
}

void PurpleCtrl::set_start_dur() {
  ipurple.set_startup(start_dur->value);
}

struct Green: Instrument {
  int &b1;
  int16 noisebuf[4][sbm];
  void fill_nbuf(int16 *noiseb,int nr,int ampl) {
    int n,lst_n,n1,
        dif,
        nxtval=0,lstval=0,val,
        brk=sbm >> nr;
    noiseb[0]=0;
    for (n=brk,lst_n=0;n<sbm;n+=brk) {
      lstval=nxtval;
      if (n==sbm-brk) nxtval=0;
      else nxtval=rand()%(ampl*2) - ampl;
      dif=nxtval-lstval << nr;
      for (n1=lst_n;n1<=n;++n1)
        noiseb[n1]=noiseb[lst_n]+(dif*(n1-lst_n) >> sshift);
      lst_n=n;
    }
    for (n=0;n<sbm;++n) {  // clipping
      val=noiseb[n];
      if (val>sbm/2) noiseb[n]=sbm/2; 
      if (val<-sbm/2) noiseb[n]=-sbm/2; 
    }
  }
  Green(int& b):Instrument(0,0),b1(b) {
    fill_nbuf(noisebuf[0],2,sbm*2);
    fill_nbuf(noisebuf[1],3,sbm*3/2);
    fill_nbuf(noisebuf[2],4,sbm);
    fill_nbuf(noisebuf[3],5,sbm*2/3);
  }
  void nxt_val(float freq,float &ind_f,float &) {
    ind_f += freq;
    int ind=static_cast<int>(ind_f);
    if (ind >= sbm) { ind-=sbm; ind_f=ind; }
    static int &tone=appWin->green_control->tone->value;
    b1=noisebuf[tone][ind];
  }
};

struct iGreen: Instrument {
  int act_buf,
      b;
  Green *act_instr;
  iGreen():
      Instrument(0,1),
      act_buf(0),
      act_instr(new Green[noise_bufs_max](b)) {
    static Sound ad[]={ Sound(&b,sbm/4),Sound(&b,sbm/2),Sound(&b,sbm*3/4),Sound(&b,sbm) };
    attack_data=ad;
    sbuf=new Sound(&b,sbm);
    static Sound dd[]={ Sound(&b,sbm),Sound(&b,sbm*7/10),Sound(&b,sbm*4/10),Sound(&b,sbm*2/10) };
    decay_data=dd;
    // attack, decay not set
  }
  void init(int dim) {
    Green *ai;
    buf=new int16[dim];
    for (int n=0;n<noise_bufs_max;++n) {
      ai=act_instr+n;
      ai->buf=buf; // no memory allocated
      ai->sbuf=sbuf;
      ai->attack_data=attack_data;
      ai->decay_data=decay_data;
    }
  }
  Instrument *get_act_instr() {
    if (++act_buf>=noise_bufs_max) act_buf=0;
    return act_instr + act_buf;
  }
} igreen;

void GreenCtrl::set_attack() {
  igreen.set_attack(attack->value);
}

void GreenCtrl::set_decay() {
  igreen.set_decay(decay->value);
}

void RedCtrl::set_startup() {
  ired.set_startup(startup->value);
}

void RedCtrl::set_decay() {
  ired.set_decay(decay->value);
}

void RedCtrl::set_start_amp() {
  ired.set_start_ampl(start_amp->value);
}

void OrangeCtrl::set_attack() {
  iorange.set_attack(attack->value);
}

void OrangeCtrl::set_decay() {
  iorange.set_decay(decay->value);
}

void RedCtrl::set_start_timbre() {
  ired.set_start_timbre(start_timbre->value-1,start_timbre->valueY-2);
}

void RedCtrl::set_timbre() {
  ired.set_timbre(timbre->value-1,timbre->valueY-2);
}

void OrangeCtrl::set_timbre() {
  iorange.set_timbre(timbre->value-1,timbre->valueY-2);
}

struct iSampled: Instrument {
  iSampled(): Instrument(0,0) { }
} isampled;


Instrument *col2instr(int note_col) {
  switch (note_col) {
    case eBlack:  return &iblack;
    case eRed:    return &ired;
    case eOrange: return &iorange;
    case eGreen:  return &igreen;
    case eBlue:   return &iblue;
    case eBrown:  return &ibrown;
    case ePurple: return &ipurple;
    default: alert("instr %d?",note_col); return &iblack;
  }
}

int col2wav_nr(int note_col) {
  switch (note_col) {
    case eBlack:  return 1;
    case eRed:    return 2;
    case eGreen:  return 3;
    case eBlue:   return 4;
    case eBrown:  return 5;
    case ePurple: return 6;
    case eOrange: return 7;
    default: alert("wave %d?",note_col); return 1;
  }
}

void connect_stereo(int col,int *val) {
  col2instr(col)->s_loc = val;
}

NoteBuffer::NoteBuffer():
    len(20),
    notes(new Note[20]) {
  reset();
}

void MidiNoteBuf::reset() {
  occ=0;
  instr=&iblack;
  ind_f=ind_f2=0;
}

void NoteBuffer::reset() {
  busy=mode=0;
  cur_note=-1;
  instr=&iblack;
  ind_f=ind_f2=0;
}

void NoteBuffer::renew() {
  int old_len=len;
  len*=2;
  Note *new_notes=new Note[len];
  for (int i=0;i<old_len;i++) new_notes[i]=notes[i];
  delete[] notes;
  notes=new_notes;
}

void NoteBuffer::report(int n) {
  printf("Voice %d\n",n);
  for (Note *np=notes;np-notes<=cur_note;np++) {
    printf("  cat=%d ",np->cat);
    if (np->cat!=eSleep) {
      printf("dur=%d ",np->dur);
      switch (np->cat) {
        case eNote:
        case ePortaNote:
          printf("freq=%0.2f col=%u ampl=%u attack=%d decay=%d ",
            np->freq,np->note_col,np->ampl,np->attack,np->decay);
      }
    }
    putchar('\n');
  }
}

float line_freq(int lnr,int sign) {  // ScLine -> frequency
  static float
/*
    c=523, a=c*5/6, bes=c*7/8, b=c*14/15, cis=c*15/14, d=c*9/8,
    dis=c*6/5, e=c*5/4, f=c*4/3, fis=c*7/5, g=c*1.5, gis=c*8/5,
*/
    a=440, bes=466.2, b=493.9, c=523.3, cis=554.4, d=587.3,
    dis=622.3, e=659.3, f=698.5, fis=740.0, g=784.0, gis=830.6,

    F[sclin_max]= {
             b*8,a*8,g*4,f*4,e*4,d*4,c*4,
             b*4,a*4,g*2,f*2,e*2,d*2,c*2,
             b*2,a*2,g,  f,  e,  d,  c,
             b,  a,  g/2,f/2,e/2,d/2,c/2,
             b/2,a/2,g/4,f/4,e/4,d/4,c/4,
             b/4,a/4,g/8,f/8,e/8,d/8,c/8,
             b/8,a/8,g/16
           },
    Fhi[sclin_max]= {
             c*8,bes*8,gis*4,fis*4,f*4,dis*4,cis*4,
             c*4,bes*4,gis*2,fis*2,f*2,dis*2,cis*2,
             c*2,bes*2,gis,  fis,  f,  dis,  cis,
             c,  bes,  gis/2,fis/2,f/2,dis/2,cis/2,
             c/2,bes/2,gis/4,fis/4,f/4,dis/4,cis/4,
             c/4,bes/4,gis/8,fis/8,f/8,dis/8,cis/8,
             c/8,bes/4,gis/16
           },
    Flo[sclin_max]= {
             bes*8,gis*4, fis*4,e*4,dis*4,cis*4,b*4,
             bes*4,gis*2, fis*2,e*2,dis*2,cis*2,b*2,
             bes*2,gis,   fis,  e,  dis,  cis,  b,
             bes,  gis/2, fis/2,e/2,dis/2,cis/2,b/2,
             bes/2,gis/4, fis/4,e/4,dis/4,cis/4,b/4,
             bes/4,gis/8, fis/8,e/8,dis/8,cis/8,b/8,
             bes/8,gis/16,fis/16
           };
  switch (sign) {
    case 0: return F[lnr]*freq_scale;
    case eHi: return Fhi[lnr]*freq_scale;
    case eLo: return Flo[lnr]*freq_scale;
    default: alert("sign=%u?",sign); return F[lnr]*freq_scale;
  }
}

uint8 lnr_to_midinr(int lnr,int sign) {  // ScLine -> midi note number
  int ind=lnr%7;
  //                b a g f e d c
  static int ar[]={ 0,2,4,6,7,9,11 };
  int nr = ar[ind] + (sign==eHi ? -1 : sign==eLo ? 1 : 0) + (lnr-ind)/7*12;
  // middle C: AMC: ind=6, nr=11+14/7*12=35
  //           midi: 60
  return 95-nr; // 60=95-35
}

void midinr_to_lnr(uint8 mnr,int& lnr,int& sign) {
                  // c  cis  d  es   e   f  fis  g  gis  a  bes  b 
  static int ar1[]={ 0 , 0 , 1 , 2 , 2 , 3 , 3 , 4 , 4 , 5 , 6 , 6 },
             ar2[]={ 0 ,eHi, 0 ,eLo, 0 , 0 ,eHi, 0 ,eHi, 0 ,eLo, 0 };
  int ind=mnr%12;
  lnr=48 - mnr/12*7 - ar1[ind];
  sign=ar2[ind];
}

int same_color(ScSection *fst,ScSection *sec,ScSection *end,ScSection*& lst) {
  int n,
      col=sec->note_col,
      sign=sec->sign;
  lst=0;
  for (n=1;;++n,sec=fst) {
    for (;;sec=appWin->mn_buf+sec->nxt_note) {
      if (sec->note_col==col && sec->sign==sign) { sec->cat=ePlay_x; lst=sec; break; }
      if (!sec->nxt_note) return n-1;
    }
    if (sec->stacc || sec->sampled) return n;
    if (++fst>=end) return n;
    for (sec=fst;;sec=appWin->mn_buf+sec->nxt_note) {
      if (sec->cat==ePlay) break;
      if (!sec->nxt_note) return n;
    }
  }
}

struct NoteBuffers {
  int voice;
  NoteBuffer nbuf[voice_max];
  void reset() {
    voice=nop;
    for (int i=0;i<voice_max;i++) nbuf[i].reset();
  }
  NoteBuffers() { reset(); }
  void find_free_voice(int start_del,int& v,int& pause) {
    if (voice==nop) {
      pause=start_del-nbuf[0].busy; v=voice=0; return;
    }
    int i,n;
    for (i=0;i<voice_max;i++) {
      n=nbuf[voice].busy;
      if (n<=0) { pause=start_del-n; v=voice; return; } // free voice found
      if (++voice>=voice_max) voice=0;
    }
    alert("voices > %d",voice_max);
    v=nop;
  }
  bool fill_note_bufs(int play_start,int play_stop);
} nbufs;

struct MidiNoteBufs {
  int mk_voice,
      key_arr[128]; // mapping midi nr -> nbuf index
  MidiNoteBuf nbuf[mk_voice_max];
  void reset() {
    int i;
    mk_voice=nop;
    for (i=0;i<128;++i) key_arr[i]=nop;
    for (i=0;i<mk_voice_max;i++) nbuf[i].reset();
  }
  MidiNoteBufs() {
    reset();
  }
  void note_on(int instr,int midi_nr,int ampl) {
    int ind,lnr,sign;
    midinr_to_lnr(midi_nr,lnr,sign);
    if (lnr<0 || lnr>=sclin_max) return;
    for (ind=0;;++ind) {
      if (ind==mk_voice_max) return; // no warning
      if (!nbuf[ind].occ) break;
    }
    MidiNoteBuf *nb=nbuf+ind;
    nb->occ=2;
    nb->start_snr=cur.snr2;
    nb->turn_s=keyboard_tune.turn_start;
    nb->lnr=lnr;
    nb->freq=line_freq(lnr,sign);
    nb->sign=sign;
    key_arr[midi_nr]=ind;
  }
  void note_off(int instr,int midi_nr) {
    if (key_arr[midi_nr]>nop) {
      MidiNoteBuf *nb=nbuf+key_arr[midi_nr];
      nb->occ=1;
      keyboard_tune.add(nb->lnr,nb->sign,nb->turn_s, nb->start_snr, cur.snr2);
    }
    key_arr[midi_nr]=nop;
  }
} mk_nbufs;

void noteOn(int instr,int midi_nr,int ampl) {
  mk_nbufs.note_on(instr,midi_nr,ampl);
}

void noteOff(int instr,int midi_nr) {
  mk_nbufs.note_off(instr,midi_nr);
}

void create_midi() { // supposed: midiout_okay = true
  Note *note;
  int voice;
  const int mult=450/appWin->act_tempo;
  for (voice=0;voice<voice_max;++voice) {
    for (note=NOTES[voice];note;++note) {
      switch (note->cat) {
        case ePause:
        case eSleep:
          if (note->cat==eSleep) goto next_voice;
          break;
        case eSampled:
        case eNote:
        case ePortaNote:
          midi_events.insert(PtrToNote(note, note->act_snr*mult, 0x90), true);
          midi_events.insert(PtrToNote(note, (note->act_snr+note->dur/tscale)*mult, 0x80), true);
          break;
      }
    }
    next_voice:;
  }
  SLList_elem<PtrToNote> *ptr;
  for (ptr=midi_events.lis;ptr;ptr=ptr->nxt) {
    if (debug) printf("ev: %d tag:%x\n",ptr->d.ev_time,ptr->d.midi_tag);
    note=ptr->d.not;
    midi_out.note_onoff(ptr->d.midi_tag, note->note_col, note->cat==eSampled,
                        ptr->d.ev_time, note->act_lnr, note->ampl);
  }
  midi_out.close();
  midi_events.reset();
  alert("midi file %s created",midi_out_file);
  midiout_okay=false;
}

bool NoteBuffers::fill_note_bufs(int play_start,int play_stop) {
  int n,
      samecol=0,
      v,   // voice
      lnr,snr,
      pause,
      decay;

  ScSection *sec,*lst_sect;
  Note *note;
  NoteBuffer *lst_nb=0;
  reset();

  for (snr=0;snr<play_start && snr<appWin->cur_score->len;++snr) // initialisation
    if (!exec_info(snr,appWin->cur_score)) return false;

  for (snr=play_start;snr<appWin->cur_score->len;) {
    if (play_stop>0 && snr>=play_stop || snr==appWin->cur_score->end_sect) {
      keyboard_tune.tune_wid=snr-play_start;
      break;
    }
    if (!exec_info(snr,appWin->cur_score))  // there might be timing, decay modifications
      return false;  
    for (lnr=0;lnr<sclin_max;lnr++) {
      ScSection *const sect=appWin->cur_score->lin[lnr]->sect;
      for (sec=sect+snr;;sec=appWin->mn_buf+sec->nxt_note)
        if (sec->cat==ePlay || !sec->nxt_note) break;
      if (sec->cat==ePlay) {
        for (;;) {
          find_free_voice((sect+snr)->del_start,v,pause); // uses: busy
          if (v==nop) break;
          NoteBuffer *const nb=nbuf+v;
          lst_nb=nb;
          samecol=same_color(sect+snr,sec,sect+appWin->cur_score->len,lst_sect);
          nb->busy=samecol*subdiv + lst_sect->del_end;
          //say("st=%d end=%d busy=%d pause=%d",sect[snr].del_start,lst_sect->del_end,nb->busy,pause);
          nb->instr=col2instr(sec->note_col);
          if (pause > 0) {
            if (++nb->cur_note >= nb->len) nb->renew();
            note=nb->notes + nb->cur_note;
            note->cat=ePause;
            note->set_timing(tscale*pause);
          }
          if (++nb->cur_note>=nb->len) nb->renew();
          note=nb->notes+nb->cur_note;
          note->act_snr=snr;
          note->note_col=sec->note_col;
          note->act_instr=nb->instr->get_act_instr();
          note->ampl=sec->note_ampl;
          note->freq=line_freq(lnr,sec->sign);
          note->act_lnr=lnr_to_midinr(lnr,sec->sign);
          if (sec->sampled) {
            if (!raw_data_okay) {
              if (!fill_raw_data(appWin->app_file,sbm)) {
                alert("wave files not read");
                return false;
              }
              appWin->PostMessage('uraw');
            }
            note->cat=eSampled;
            RawData *raw=RAW+col2wav_nr(sec->note_col);
            if (!raw->buf) {
              alert("sampled note: empty buffer");
              return false;
            }
            int dur=raw->size * appWin->act_tempo;
            note->set_timing(dur - dur % tscale + tscale);
            note->attack=note->decay=0;
            nb->busy=max(0,note->dur / tscale);
          }
          else {
            note->cat= lst_sect->port_dlnr ? ePortaNote : eNote;
            note->set_timing(tscale * (samecol*subdiv - sect[snr].del_start + lst_sect->del_end));
            if (nb->instr->attack) {
              note->attack=tscale * nb->instr->attack / at_scale;
              if (note->attack > note->dur) note->attack=note->dur;
            }
            else note->attack=0;
            if (note->cat==ePortaNote) {
              int dlnr,
                  dsnr,
                  new_snr=snr,
                  new_lnr=lnr;
              ScSection *new_sect;
              Note *new_note;
              for (;;) {
                dlnr= lst_sect->dlnr_sign ? lst_sect->port_dlnr : -lst_sect->port_dlnr;
                dsnr=lst_sect->port_dsnr;
                new_snr+=samecol+dsnr;
                new_lnr+=dlnr;
                new_sect=appWin->cur_score->lin[new_lnr]->sect;
                for (sec=new_sect+new_snr;;sec=appWin->mn_buf+sec->nxt_note)
                  if (sec->cat==ePlay && sec->note_col==note->note_col || !sec->nxt_note) break;
                if (sec->cat==ePlay) {
                  note->decay=tscale * (dsnr * subdiv - lst_sect->del_end + sec->del_start);
                  samecol=same_color(new_sect+new_snr,sec,new_sect+appWin->cur_score->len,lst_sect);
                  if (++nb->cur_note>=nb->len) nb->renew();
                  new_note=nb->notes+nb->cur_note;
                  new_note->act_snr=new_snr;
                  new_note->note_col=note->note_col;
                  new_note->act_instr=note->act_instr;
                  new_note->ampl=note->ampl;
                  new_note->cat= lst_sect->port_dlnr ? ePortaNote : eNote;
                  new_note->set_timing(
                    tscale * (samecol*subdiv - sec->del_start + lst_sect->del_end));
/*
                  if (nb->instr->attack) {
                    new_note->attack=tscale * nb->instr->attack / at_scale;
                    if (new_note->attack > new_note->dur) new_note->attack=new_note->dur;
                  }
                  else
*/
                  new_note->attack=0;
                  new_note->act_lnr=lnr_to_midinr(dlnr+lnr,sec->sign);
                  new_note->freq=line_freq(new_lnr,sec->sign);
                  if (new_note->cat==eNote) {
                    if (nb->instr->decay) {
                      if (lst_sect->stacc) { decay=1; new_note->decay=tscale; }
                      else {
                        decay=nb->instr->decay; 
                        new_note->decay=tscale * decay;
                      }
                    }
                    else
                      new_note->decay=decay=0;
                    nb->busy=(new_snr-snr+samecol)*subdiv+decay;
                    break;
                  }
                }
                else {
                  alert("unterminated portando");
                  note->decay=tscale;
                  nb->busy=(new_snr-snr+samecol)*subdiv+1;
                  break;
                }
                note=new_note;
              }
            }
            else if (nb->instr->decay) {
              if (lst_sect->stacc) { nb->busy+=1; note->decay=tscale; }
              else {
                decay=nb->instr->decay; nb->busy+=decay;
                note->decay=tscale * decay;
              }
            }
            else note->decay=0;
          }
          if (!sec->nxt_note) break;
          sec=appWin->mn_buf+sec->nxt_note;
        }
      }
    }
    for (v=0;v<voice_max;v++) nbuf[v].busy-=subdiv;
    ++snr;
  }
  if (!lst_nb) {
    alert("empty score");
    return false;
  }

  NoteBuffer *nb1;
  if (play_stop>0 || appWin->cur_score->end_sect > nop) {
    find_free_voice(0,v,pause);   // add pause note at end of last NoteBuffer
    nb1=nbuf+v;
    if (++nb1->cur_note>=nb1->len) nb1->renew();
    note=nb1->notes+nb1->cur_note;
    note->cat=ePause;
    note->set_timing(pause*tscale);
  }
  for (v=0;v<voice_max;++v) { // all note bufs end with cat=eSleep
    nb1=nbuf+v;
    if (++nb1->cur_note>=nb1->len) nb1->renew();
    nb1->notes[nb1->cur_note].cat=eSleep;
  }
  if (debug) {
    puts("-----------");
    for (v=0;v<voice_max;++v) nbuf[v].report(v);
  }
  for (v=0;v<voice_max;++v) NOTES[v]=nbufs.nbuf[v].notes; // nbuf[v] may have been realloced
  exec_info(0,appWin->cur_score);  // maybe initialize timing
  return true;
}

void Sampler::playScore(int play_start,int play_stop) {
  cur.snr=cur.snr2=cur.tcount=0;
  cur.snr_start=play_start;

  for (int n=0;n<colors_max;++n) {       // reset instr->d_buf's
    Instrument *instr=col2instr(colors[n]);
    for (int n1=0;n1<stereo_delay;++n1) instr->d_buf[n1]=0;
  }

  bool ok=nbufs.fill_note_bufs(play_start,play_stop);
  int lnr,snr;
  ScSection *sec;

  for (lnr=0;lnr<sclin_max;lnr++) { // restore ePlay_x notes
    ScSection *const sect=appWin->cur_score->lin[lnr]->sect;
    for (snr=play_start;snr<appWin->cur_score->len;++snr) {
      for (sec=sect+snr;;sec=appWin->mn_buf+sec->nxt_note) {
        if (sec->cat==ePlay_x) sec->cat=ePlay;
        if (!sec->nxt_note) break;
      }
    }
  }
  if (!ok) return;

  if (midiout_okay) create_midi();
  resume_thread(appWin->sampler_id);
}

void BufferProc(void*,void* buffer,size_t,const media_raw_audio_format&) {
  int n,n1,
      ind_wave,
      value,
      mix;
  Sound *snd1,*snd2;
  Note *note;
  NoteBuffer *nbuf;
  MidiNoteBuf *mk_nbuf;
  Instrument *instr;
  int tempo=appWin->act_tempo;
  bool stop_req=true;
  RawData *raw;
  n=cur.tcount*tempo*IBsize/40000;
  if (cur.tcount==0 || n>cur.snr2) {
    cur.snr2=n;
    if (cur.snr2 % appWin->act_meter == 0) {  // update measure nr display
      BMessage csnr('csnr');
      csnr.AddInt32("",cur.snr2+cur.snr_start);
      appWin->PostMessage(&csnr);
    }
  }
  ++cur.tcount;
  for (n=0;n<colors_max;++n) {       // reset instr->buf's
    instr=col2instr(colors[n]);
    for (n1=0;n1<IBsize;++n1) instr->buf[n1]=0;
    //for (n1=0;n1<stereo_delay;++n1) instr->d_buf[n1]=0;
  }
  for (n1=0;n1<IBsize;++n1) isampled.buf[n1]=0;
  if (mk_connected)
    for (int mk_voice=0;mk_voice<mk_voice_max;++mk_voice) { // midi keyboard
      mk_nbuf=mk_nbufs.nbuf+mk_voice;
      if (mk_nbuf->occ) {
        instr=mk_nbuf->instr;
        for (n=0;n<IBsize;++n) {
          instr->nxt_val(mk_nbuf->freq,mk_nbuf->ind_f,mk_nbuf->ind_f2);
          snd1=instr->sbuf;
          value=snd1->wave[0] * snd1->ampl;
          if (mk_nbuf->occ==1)
            value *= (IBsize-n)/IBsize;
          instr->buf[n]+=ampl2ampl(4,value>>sshift);
        }
        if (mk_nbuf->occ==1) mk_nbuf->occ=0;
      }
    }
  for (int voice=0;voice<voice_max;++voice) {
    nbuf=nbufs.nbuf+voice;
    ind_instr_buf=-1;
    note=NOTES[voice];
    loop_start:
    ++ind_instr_buf;
    switch (note->cat) {
      case eSleep:
        break;
      case ePause:
        stop_req=false;
        for (;ind_instr_buf<IBsize;++ind_instr_buf) {
          n=note->remain;
          if (n > 0) note->remain-=tempo;
          else {
            note = ++NOTES[voice];
            note->remain += n;
            goto loop_start;
          }
        }
        break;
      case eSampled:
        stop_req=false;
        raw=RAW+col2wav_nr(note->note_col);
        for (;ind_instr_buf<IBsize;++ind_instr_buf) {
          n=(note->dur-note->remain)/tempo;
          if (n>=0 && n<raw->size) {
            value=raw->buf[n];
            isampled.buf[ind_instr_buf]+=ampl2ampl(note->ampl,value);
          }
          n=note->remain;
          if (n > 0) note->remain -= tempo;
          else {
            note = ++NOTES[voice];
            note->remain += n;
            goto loop_start;
          }
        }
        break;
      case eNote:
      case ePortaNote:
        stop_req=false;
        if (cur.snr < note->act_snr) {
          for (++cur.snr;;++cur.snr) {
            if (!exec_info(cur.snr,appWin->cur_score)) { appWin->stop_requested=true; break; }
            if (cur.snr == note->act_snr) break;
          }
        }
        instr=note->act_instr;
        for (;ind_instr_buf<IBsize;++ind_instr_buf) {
          if (note->remain>=0) {
            instr->nxt_val(note->freq,nbuf->ind_f,nbuf->ind_f2);
            if (note->attack) {
              div_t d=div(max(0,ad_max * (note->dur - note->remain)),note->attack);
              n=d.quot;
              if (n >= ad_max-1) {
                snd1=instr->attack_data + ad_max-1;
                value=snd1->wave[0] * snd1->ampl;
              }
              else {
                snd1=instr->attack_data + n;
                snd2=snd1+1;
                mix=(d.rem << sshift)/note->attack;
                value=snd1->wave[0] * snd1->ampl * (sbm-mix) +
                      snd2->wave[0] * snd2->ampl * mix >> sshift;
              }
            }
            else {
              snd1=instr->sbuf;
              value=snd1->wave[0] * snd1->ampl;
            }
          }
          else if (note->cat==ePortaNote) {
            Note *new_note=note+1;
            if (note->decay) {
              float fmix=float(-note->remain)/float(note->decay);
              instr->nxt_val(note->freq*(1.0-fmix) + new_note->freq*fmix,nbuf->ind_f,nbuf->ind_f2);
            }
            else instr->nxt_val(note->freq,nbuf->ind_f,nbuf->ind_f2);
            snd1=instr->sbuf;
            value=snd1->wave[0] * snd1->ampl;
          }
          else if (note->decay) {
            instr->nxt_val(note->freq,nbuf->ind_f,nbuf->ind_f2);
            div_t d=div(max(0,-ad_max*note->remain),note->decay);
            n=d.quot;
            snd1=instr->decay_data + n;
            mix=(d.rem << sshift)/note->decay;
            if (n<ad_max-1) {
              snd2=snd1+1;
              value=snd1->wave[0] * snd1->ampl * (sbm-mix) +
                    snd2->wave[0] * snd2->ampl * mix >> sshift;
            }
            else if (n==ad_max-1)
              value=snd1->wave[0] * snd1->ampl * (sbm-mix) >> sshift;
            else value=0;
          }
          else value=0;
          instr->buf[ind_instr_buf]+=ampl2ampl(note->ampl,value>>sshift);
          n=note->remain + note->decay;
          if (n > 0) note->remain-=tempo;
          else {
            if (note->cat!=ePortaNote) nbuf->ind_f=nbuf->ind_f2=0;
            note = ++NOTES[voice];
            note->remain += n;
            goto loop_start;
          }
        }
        break;
      default:
        alert("unknown note %d",note->cat);
    }
  }
  for (n=0;n<IBsize;n++) tmp_buf_r[n]=tmp_buf_l[n]=0;
  int val_r,val_l,
      stereo_loc;
  int16 *bp;
  static int nr2stereo[]={ eLeft,0,eRight };
  for (n=0;n<=colors_max;++n) { // read instr->buf's
    instr= n==colors_max ? &isampled : col2instr(colors[n]);
    bp=instr->buf;
    stereo_loc=nr2stereo[*instr->s_loc];
    for (n1=0;n1<IBsize;++n1) {
      switch (stereo_loc) {
        case eLeft:
          val_r=bp[n1]; val_l=instr->delay(val_r);
          break;
        case eRight:
          val_l=bp[n1]; val_r=instr->delay(val_l);
          break;
        default:
          val_l=val_r=bp[n1];
      }
      if (val_r!=0) tmp_buf_r[n1]+=val_r; // instr->pprocess(val_r);
      if (val_l!=0) tmp_buf_l[n1]+=val_l; // instr->pprocess(val_l);
    }
  }
  float val1_r,val1_l;
  static float val2_r,val2_l;
  float *buf=static_cast<float*>(buffer);
  for (n=0;n<IBsize;++n) {
    val1_r=val2_r; val1_l=val2_l;
    val2_r=tmp_buf_r[n]/(float)sbm; val2_l=tmp_buf_l[n]/(float)sbm;
    n1=n<<2;
    buf[n1]  =(val1_r+val2_r)*0.5; buf[n1+2]=val2_r;
    buf[n1+1]=(val1_l+val2_l)*0.5; buf[n1+3]=val2_l;
  }
  if (dumpwav_okay) {
    float sig;
    for (n=0;n < IBsize<<1;++n) {
      sig=buf[n<<1];
      wave_buf[n]=static_cast<int16>(32000. * (sig>1.0 ? 1.0 : sig<-1.0 ? -1.0 : sig));
    }
    if (!dump_wav((char*)wave_buf,IBsize<<2)) {
      alert("dump wave problem");
      dumpwav_okay=false;
    }
  }
  // say("t: %d",appWin->sampler->player->CurrentTime());
  float *f=appWin->scope_buf.buf[appWin->scope_buf.get_prev()];
  if (appWin->scope_range->value) // all
    for (n=0;n<scope_win;++n) f[n]=buf[n<<5];
  else
    for (n=0;n<scope_win;++n) f[n]=buf[n<<2];
  appWin->PostMessage('scop');
  if (appWin->stop_requested) {
    if (keyboard_tune.cur_ind>=0)
      appWin->PostMessage('c_kt'); // copy keyboard tune
    if (appWin->cur_score->is_music)
      appWin->PostMessage('upsl'); // update sliders
    resume_thread(appWin->sampler_id);
  }  
  else if (stop_req) {
    if (!appWin->cur_score->is_music && appWin->repeat && *appWin->repeat) {
      appWin->PostMessage('repl');  // replay
    }
    else {
      if (appWin->cur_score->is_music)
        appWin->PostMessage('upsl'); // update sliders
    }
    resume_thread(appWin->sampler_id);
  }
}

Sampler::Sampler() {
  appWin->sampler=this;
  player=new BSoundPlayer("player",BufferProc,0,0);
  if (player->InitCheck()!=B_OK) debugger("BSoundPlayer problem");
  if (player->Format().channel_count!=2) debugger("Chan count != 2");
  Fsize=player->Format().buffer_size/sizeof(float); // = 16384/4 = 4096
  IBsize=Fsize/4;      // = 1024
  scope_win=IBsize>>3; // = 128, display in oscilloscope
  appWin->scope_buf.set_buf(scope_win);
  tmp_buf_r=new int[IBsize];
  tmp_buf_l=new int[IBsize];
  wave_buf=new int16[IBsize<<1];
  for (int n=0;n<colors_max;++n)    // set instr->buf's
    col2instr(colors[n])->init(IBsize);
  isampled.init(IBsize);
  for (;;) {
    suspend_thread(appWin->sampler_id); // awakened by sampler->playScore()
    player->Start();
    player->SetHasData(true);
    suspend_thread(appWin->sampler_id); // awakened by BufferProc()
    player->Stop();
  }
}
