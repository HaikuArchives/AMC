const int STORE_SEL='scol',    // collect selected notes (message scheme -> amc)
          CONTENTS_SEL='scon', // contents of selected notes (reply amc -> scheme)
          SEND_NOTES='sndn',   // send notes (message scheme -> amc)
          ALIVE='aliv';        // amc alive (messages scheme <-> amc)
const char *SELD_NOTES="seld-notes",
           *SENT_NOTES="sent-notes",
           *SETTINGS="settings";
const int stored_notes_max=100; // stored notes

struct StoredNote {
  int8 lnr,  // line number
       dur,  // note duration
       sign; // -1,0,1 (flat, normal, sharp)
  int16 snr; // section number
};

struct Settings {
  int8 meter;
  Settings(int);
};
