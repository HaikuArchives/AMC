#include <SoundPlayer.h>
const int
  subdiv=3;        // time units pro score segment

struct Sampler {
  BSoundPlayer *player;
  Sampler();
  void playScore(int start,int stop);
};

struct RedCtrl: StdView {
  RedCtrl(BRect);
  HVSlider *start_timbre,
           *timbre;
  HSlider *startup,
          *decay;
  VSlider *start_amp;
  void set_startup();
  void set_decay();
  void set_start_amp();
  void set_start_timbre();
  void set_timbre();
  void redraw_sliders();
  void Draw(BRect);
};

struct OrangeCtrl: StdView {
  OrangeCtrl(BRect);
  HVSlider *timbre;
  HSlider *attack,
          *decay;
  void set_attack();
  void set_decay();
  void set_timbre();
  void redraw_sliders();
  void Draw(BRect);
};

struct BlackCtrl: StdView {
  BlackCtrl(BRect);
  HVSlider *fm_ctrl;
  HSlider *detune,*attack,*decay;
  double act_freq,display_mod; // set by set_fm()
  CheckBox *sub_band;
  void set_fm(int);
  void set_detune();
  void set_attack();
  void set_decay();
  void redraw_sliders();
};

struct BrownCtrl: StdView {
  BrownCtrl(BRect,BrownCtrl*&);
  HVSlider *fm_ctrl;
  HSlider *detune,*attack,*decay;
  double act_freq,display_mod; // set by set_fm()
  CheckBox *sub_band;
  void set_fm(int);
  void set_detune();
  void set_attack();
  void set_decay();
  void redraw_sliders();
};

struct GreenCtrl: StdView {
  GreenCtrl(BRect);
  HSlider *attack, *decay;
  VSlider *tone;
  void set_attack(), set_decay();
  void redraw_sliders();
};

struct PurpleCtrl: StdView {
  PurpleCtrl(BRect);
  VSlider *harm[harm_max];    // harmonics
  VSlider *st_harm[harm_max]; // startup harmonics
  HSlider *start_dur;           // attack duration
  void set_hs_ampl(int*);
  void set_st_hs_ampl(int*);
  void set_start_dur();
  void Draw(BRect);
  void redraw_sliders();
};

struct BlueCtrl: StdView {
  BlueCtrl(BRect);
  HSlider *attack,*decay;
  CheckBox *chorus,*rich,*p_attack;
  void set_attack();
  void set_decay();
  void set_piano();
  void redraw_sliders();
};

struct ShowSampled: StdView {
  BFont *small_font;
  ShowSampled(BRect);
  void Draw(BRect);
};

struct KeybNote {
  int lnr,snr,sign,dur;
};

struct KeybTune {
  KeybNote buf[1000];
  int cur_ind,
      turn_start,
      tune_wid;
  void reset();
  void nxt_turn();
  KeybTune();
  void add(int lnr,int sign,int turn_s,int snr1,int snr2);
};

bool exec_info(int,Score*);
void connect_stereo(int color,int* val);
int col2wav_nr(int note_col);

extern KeybTune keyboard_tune;
extern bool mk_connected,
            dumpwav_okay,
            midiout_okay;
extern const char *midi_out_file;
 
