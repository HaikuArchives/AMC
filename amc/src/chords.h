struct ChordsWindow: BWindow {
  RButtonArrowCtrl *tone,
                   *chord;
  Array<int,10>chord_notes;
  int32 the_key_nr,
        the_chord,
        the_distance;
  struct ToneData {
    float y;
    char *majkey,
         *minkey,
         *nr_signs;
    BBitmap *sign;
  } *toneData;
  struct ChordsView *view;
  void assign_notes(int);
  ChordsWindow(BPoint top);
  void MessageReceived(BMessage* mes);
};
