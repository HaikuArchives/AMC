#include <stdio.h>
#include <SupportDefs.h>
#include "midi-out.h"
#include "colors.h"

static FILE *dumpfd;
static int32 data_size,
             cur_time;
static bool init_ok;
MidiOut midi_out;

static uint8 *reord16(uint16 n) {
  static uint8 buf[2];
  buf[0]=(n>>8) & 0xff;
  buf[1]=n & 0xff;
  return buf;
}

static uint8 *reord8(uint8 n) {
  static uint8 buf[1];
  buf[0]=n;
  return buf;
}

static uint8 *reord32(uint32 n) {
  static uint8 buf[4];
  buf[0]=(n>>24) & 0xff;
  buf[1]=(n>>16) & 0xff;
  buf[2]=(n>>8) & 0xff;
  buf[3]=n & 0xff;
  return buf;
}

static void wr(uint8 *n,int nr) {
  data_size+=nr;
  fwrite(n, nr,1,dumpfd);
}

static void del_0() {
  uint8 i8=0;
  wr(&i8, 1);
}

static void var_int(uint32 val) {
  uint8 bytes[5];
  uint8 i8;
  bytes[0] = (val >> 28) & 0x7f;    // most significant 5 bits
  bytes[1] = (val >> 21) & 0x7f;    // next largest 7 bits
  bytes[2] = (val >> 14) & 0x7f;
  bytes[3] = (val >> 7)  & 0x7f;
  bytes[4] = (val)       & 0x7f;    // least significant 7 bits

  int start = 0;
  while (start<5 && bytes[start] == 0)  start++;

  for (int i=start; i<4; i++) {
    i8=bytes[i] | 0x80;
    wr(&i8, 1);
  }
  i8=bytes[4];
  wr(&i8, 1);
}

MidiOut::MidiOut() {
  int n;
  for (n=0;n<col_max;++n) col2instr[n]=-1;
  for (n=0;n<col_max;++n) col2note[n]=-1;
  col2instr[eBlack] = 6-1;   // electric piano 2
  col2instr[eRed]   = 25-1;  // guitar
  col2instr[eGreen] = 41-1;  // violin
  col2instr[eBlue]  = 0;     // piano
  col2instr[eBrown] = 5-1;   // electric piano 1
  col2instr[ePurple]= 17-1;  // drawbar organ
  col2instr[eOrange]= 26-1;  // guitar

  col2note[eBlack] = 36;  // bass drum
  col2note[eRed]   = 47;  // low-mid tom
  col2note[eGreen] = 38;  // snare
  col2note[eBlue]  = 46;  // open hi-hat
  col2note[eBrown] = 42;  // closed hi-hat
  col2note[ePurple]= 62;  // mute high conga
  col2note[eOrange]= 39;  // hand clap
}

bool MidiOut::init(const char *fn) {     // sets init_ok if succesful
  init_ok=false;
  if ((dumpfd=fopen(fn,"w"))==0) return false;
  cur_time=0;
  init_ok=true;
  fwrite("MThd", 4,1,dumpfd); // header
  wr(reord32(6), 4);          // header size
  wr(reord16(0), 2);          // file type
  wr(reord16(1), 2);          // # tracks
  wr(reord16(0x80), 2);       // time format

  fwrite("MTrk", 4,1,dumpfd); // track data
  wr(reord32(0x100), 4);      // track size (temporary)
  data_size=0;

  for (int n=0;n<col_max;++n)
    if (col2instr[n]>=0) {
      del_0();
      wr(reord8(0xb0 + n),1);
      wr(reord8(0x40),1);  // damper on/off
      wr(reord8(0),1);     // damped
      //wr(reord8(64),1);  // not damped

      del_0();
      wr(reord8(0xc0 + n),1);        // select instrument channel
      wr(reord8(col2instr[n]),1);  // instrument
    }
  del_0();
  wr(reord8(0xc9),1);       // select instrument channel 10
  wr(reord8(0),1);               // instrument
  return true;
}

int ampl2ampl[ampl_max+1] = { 0,0x20,0x30,0x40,0x50,0x60,0x70 }; // max: 0x7f

int note2note(int n) {         // midi: middle C = 60, AMC: middle C = 20
  if (n<0 || n>=100) { puts("midi: bad note"); return 60; }
  return n;
}

void MidiOut::note_onoff(uint8 tag,int color,bool sampled,int tim,int nr,int ampl) {
  if (!init_ok) return;
  if (tim==cur_time)
    del_0();
  else {
    var_int(tim-cur_time);
    cur_time=tim;
  }
  if (sampled) {
    wr(reord8(tag+9),1);          // note on, channel #
    wr(reord8(col2note[color]),1);
  }
  else {
    wr(reord8(tag+color),1);       // note on, channel #
    wr(reord8(note2note(nr)),1);   // note nr
  }
  wr(reord8(tag==0x80 ? 0 : ampl2ampl[ampl]),1); // velocity
}

void MidiOut::close() {
  if (!init_ok) return;
  del_0();
  wr(reord8(0xff),1);
  wr(reord8(0x2f),1);
  wr(reord8(0),1);
  fseek(dumpfd, 18, SEEK_SET);
  wr(reord32(data_size), 4);
  fclose(dumpfd);
  init_ok=false;
}
/*
int main() {
  midi_out.init("out.mid");
  midi_out.note_on(0,0,20,4);
  midi_out.note_off(0,200,20,0);
  midi_out.close();
  if (init_ok) exit(0); exit(1);
}
*/
