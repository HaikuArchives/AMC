struct NoteDistWindow: BWindow {
  struct NoteDistView *view;
  NoteDistWindow(BPoint top);
  void set(int,int,Score *sc);
  void MessageReceived(BMessage*);
};
