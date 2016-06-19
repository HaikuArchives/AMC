const int
  ePlay=0,
  ePlay_x=1,
  eSilent=2,
  eHi=1, // sharp
  eLo=2, // flat
  sclin_max=45,    // max lines per score
  halftones_max=sclin_max*20/12+2, // 2 half tones per line, except notes B and E
  scope_max=10,    // scope buffer
  mn_max=1000,     // multiple note buffer
  harm_max=5,      // harmonics purple instr
  times_max=50;    // max parameters of 'time:' command

struct ScoreViewBase;
struct Score;
struct ScoreView;
struct ZoomView;
struct AppView;
struct MusicView;
struct KeybTune;

struct StdView: public BView {
  StdView(BRect);
};

struct ScSection {
  uint note_col    :4,  // eBlack, eRed, ...
       cat         :2,  // ePlay, eSilent, ...
       sign        :2,  // sharp: eHi, flat: eLo, or 0
       note_ampl   :3,  // default: 4, min: 1, max: 6
       stacc       :1,  // staccato?
       sampled     :1,  // sampled note?
       sel         :1,  // selected?
       port_dlnr   :5,  // portando: delta lnr
       dlnr_sign   :1,  // delta lnr positive?
       port_dsnr   :5,  // portando: delta snr  
       del_start   :3,  // delay at section start
       del_end     :3,  // delay at section end
       nxt_note    :10; // index of mn_buf[]
  ScSection(int col,int amp);
  void drawSect(ScoreViewBase*,int snr,int lnr);
  void drawPlaySect(ScoreViewBase* theV,BPoint &start,BPoint &end);
  void drawZoomSilentSect(ZoomView* theV,rgb_color,BPoint &start,BPoint &end,ScSection *prev);
  void drawZoomPlaySect(ZoomView* theV,BPoint &start,BPoint &end,ScSection *prev,ScSection *next);
  void drawPortaLine(ScoreViewBase* theV,int snr,int lnr,bool erase);
  void drawS_ghost(ScoreViewBase*,int snr,int lnr,bool erase);
  void reset();
};  

struct ScLine {
  int& len;
  ScSection *sect;
  int8 note_sign;   // sharp or flat
  ScLine(Score*);
  void eraseScLine(ScoreViewBase*,int lnr,int left,int right);
  void drawScLine(ScoreViewBase*,int lnr,int left,int right);
  void drawZoomScLine(ZoomView*,int lnr,int left,int right);
};

template <class T,int dim>
struct Array {
  T buf[dim];
  bool in_range(int ind) {
    if (ind<0) { alert("array: index=%d",ind); return false; }
    if (ind>=dim) { alert("Array: index=%d (>=%d)",ind,dim); return false; }
    return true;
  }
  int get_index(T& memb) {
    for (int ind=0;ind<dim;++ind)
      if (buf[ind]==memb) return ind;
    return -1;
  }
  T& operator[](int ind) {
    if (in_range(ind)) return buf[ind];
    return buf[0];
  }
};

struct Score {
  char* name;
  int ncol,     // note color
      nampl,    // note amplitude
      len,      // number of sections
      lst_sect, // last written section
      end_sect, // place of end line
      signs_mode; // flats or sharps?
  const bool is_music;   // in MusicView?
  struct ScInfo* scInfo;
  Array<ScLine*,sclin_max>lin;
  RadioButton *rbut;  // located in namesView
  Score(const char *nam,int length,uint sctype);
  void copy(Score*);
  void add_copy(Score*,int,int,int,int,int,int,ScoreViewBase*);
  void reset();
  bool check_len(int);
  ScSection *get_section(int lnr,int snr);
  int to_half(int lnr,int sign);
  void from_half(int n,int& lnr2,int& sign);
  void drawText(BView*,int snr);
  void drawEnd(BView* theV);
  void time_mark(BView* theV,int snr);
  void put_chord(ScoreView*);
  void set_signs(int,int);
  void tone2signs();
  void fill_from_buffer(struct StoredNote*,int len);
  void copy_keyb_tune(KeybTune &kt);
};

struct ScopeBuf {
  float *buf[scope_max];
  int cur_buf;
  int occupied,  // nr used parts
      scope_window;
  ScopeBuf();
  int get_prev();
  void reset();
  void set_buf(const int);
};

struct AppWindow: public BWindow {
  char *app_file;
  Score *act_tune;     // set by tune name buttons
  Str script_file,
      input_file;
  int32 act_score,     // set by score choice buttons
        act_color,     // set by color choice buttons
        act_meter,     // set by meter view
        act_action,    // set by mouse action choice buttons
        prev_mess;     // previous message
  int act_tempo;       // set by tempo slider
  bool stop_requested,
       *repeat;
  AppView *appView;
  struct ChordsWindow *chordsWin;
  struct NoteDistWindow *ndistWin;
  struct EditListWindow *editListWin;
  BMessenger *msgr;
  uint sampler_id;
  struct Sampler *sampler;
  struct ScopeBuf scope_buf;
  ScSection mn_buf[mn_max];
  int mn_end;
  RButtonAM1LampCtrl *active_scoreCtrl;
  BView *act_instr_ctrl;
  struct BlackCtrl *black_control;
  struct RedCtrl *red_control;
  struct OrangeCtrl *orange_control;
  struct GreenCtrl *green_control;
  struct BlueCtrl *blue_control;
  struct PurpleCtrl *purple_control;
  struct BrownCtrl *brown_control;
  struct ShowSampled *show_sampled;
  struct InfoText *info_text;
  CheckBox *scope_range,
           *solo,
           *draw_col,
           *note_dist,
           *edit_list,
           *no_set,
           *con_mkeyb,
           *dumpwav,
           *midi_output,
           *chords,
           *use_raw;
  Score *scoreBuf,
        *cur_score;
  AppWindow(AppWindow*&,char *appf,char *inf);
  bool QuitRequested();
  void MessageReceived(BMessage* mes);
  bool exec_cmd(struct ScInfo&);
  bool save(const char*);
  bool restore(const char *file);
  bool read_script(const char *script,MusicView*);
  bool save_script(const char *script);
  bool run_script(const char *text,MusicView*);
  void set_ctrl_sliders();
  void check_reset_as();
  void modify_script(BTextView*,int start,int stop);
  bool read_tunes(const char*);
  void unfocus_textviews();
  bool find_score(Score*,struct ScoreViewStruct*&);
};

template<class T>
struct SLList_elem {    // single-linked list element
  T d;
  SLList_elem<T>* nxt;
  SLList_elem(T& d1):d(d1),nxt(0) { }
  ~SLList_elem() { delete nxt; }
};

template<class T>     // single-linked list
struct SLinkedList {
  SLList_elem<T> *lis;
  SLinkedList() { lis=0; }
  ~SLinkedList() { delete lis; }
  void reset() { delete lis; lis=0; }
  void insert(T elm, bool incr) {  // if incr then increasing
    SLList_elem<T> *p,*p1;
    if (!lis)
      lis=new SLList_elem<T>(elm);
    else if (elm==lis->d);
    else if (incr && (elm<lis->d) || !incr && (lis->d<elm)) {
      p1=new SLList_elem<T>(elm);
      p1->nxt=lis; lis=p1;
    }
    else {
      for (p=lis;;p=p->nxt) {
        if (p->d==elm) break;
        if (!p->nxt) {
          p->nxt=new SLList_elem<T>(elm);
          break;
        }
        if (incr && (elm < p->nxt->d) || !incr && (p->nxt->d < elm)) {
          p1=new SLList_elem<T>(elm);
          p1->nxt=p->nxt; p->nxt=p1;
          break;
        }
      }
    }
  }
  void remove(T elm) {
    SLList_elem<T> *p,*prev;
    if (!lis) return;
    if (lis->d==elm) {
      p=lis->nxt; lis->nxt=0; delete lis; lis=p;
      return;
    }
    for (prev=lis,p=lis->nxt;p;) {
      if (p->d==elm) {
        prev->nxt=p->nxt; p->nxt=0; delete p;
        return;
      }
      else { prev=p; p=p->nxt; }
    }
  }
  void invert() {
    SLList_elem<T> *p,*prev,*next;
    if (!lis || !lis->nxt) return;
    for (prev=lis,p=lis->nxt,next=p->nxt,lis->nxt=0;;) {
      p->nxt=prev;
      prev=p;
      if (!next) { lis=p; break; }
      p=next;
      next=p->nxt;
    }
  }
};

void alert(const char *form,...);
void say(char *form,...);
rgb_color color(int);
inline int min(int a,int b) { return a<=b ? a : b; }
inline int max(int a,int b) { return a>=b ? a : b; }
inline float fmin(float a,float b) { return a<=b ? a : b; }
inline float fmax(float a,float b) { return a>=b ? a : b; }
inline int float2int(float fl) { return static_cast<int>(fl>=0. ? fl+0.5 : fl-0.5); }
extern const int nop;
extern bool debug;
extern AppWindow *appWin;
extern BFont *appFont;
extern int colors[];
