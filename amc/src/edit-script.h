struct EditTextView: BTextView {
  struct MeasInfo {
    int pos,
        snr;
  } *meas_info;
  int cur_index;
  bool show_meas;
  EditTextView(BRect,BRect,uint32);
  bool CanEndLine(int32 offset);
  void FrameResized(float,float);
  void KeyDown(const char*,int32);
  void reset();
  void draw_meas(int);
  void clear_meas(BRect);
  void Draw(BRect);
};

struct EditScript {
  EditTextView *textview;
  BScrollView *scrollview;
  EditScript(BRect);
  void read_scriptf(FILE*);
  void write_scriptf(FILE*);
  void save_scriptf(FILE*);
  void report_meas(int pos,int snr);
};
