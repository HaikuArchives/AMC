const int col_max=10;

struct MidiOut {
  int col2instr[col_max]; // instruments
  int col2note[col_max];  // percussion channel 10
  MidiOut();
  bool init(const char*);
  void close();
  void note_onoff(uint8 tag,int color,bool sampled,int time,int nr,int ampl);
  //void note_off(int color,int time,int nr);
};

extern MidiOut midi_out;
