/*
Using buffered files.
Original authors:
   Sed Barbouky
   Christian Klein
*/
#include <stdio.h>
#include <SupportDefs.h>
#include "dump_wav.h"

static bool inited=false;
static FILE *dumpfd;
static int32 size;

// return false if error
bool init_dump_wav(const char *fname)
{
  int16 dum16;
  uint32 dum32;

  inited=false;
  if ((dumpfd=fopen(fname,"w"))==0) return false;
  if (
     fwrite("RIFF", 4,1,dumpfd)!=1 ||
     (dum32=36, fwrite(&dum32, 4,1,dumpfd)!=1) ||       // header size
     fwrite("WAVEfmt ", 8,1,dumpfd)!=1 ||
     (dum32=16, fwrite(&dum32, 4,1,dumpfd)!=1) ||       // chunk size  
     (dum16=1, fwrite(&dum16, 2,1,dumpfd)!=1) ||        // format tag (1 = uncompressed PCM)
     (dum16=1, fwrite(&dum16, 2,1,dumpfd)!=1) ||        // no of channels
     (dum32=44100, fwrite(&dum32, 4,1,dumpfd)!=1) ||    // rate
     (dum32=44100*2, fwrite(&dum32, 4,1,dumpfd)!=1) ||  // average bytes/sec
     (dum16=(16+7)/8, fwrite(&dum16, 2,1,dumpfd)!=1) || // block align
     (dum16=16, fwrite(&dum16, 2,1,dumpfd)!=1) ||       // bits per sample
     fwrite("data", 4,1,dumpfd)!=1 ||
     (dum32=0, fwrite(&dum32, 4,1,dumpfd)!=1)           // sample length (0 for now)
  ) goto error;

  inited=true;
  size=36;
  return true;

  error:
  fclose(dumpfd);
  return false;
}

bool close_dump_wav(void) {
  if (!inited) return false;
  inited=false;

  // update the wav header
  fseek(dumpfd, 4, SEEK_SET);  // first place to update
  if (fwrite(&size, 4,1,dumpfd)!=1) goto error;
  size-=36;
  fseek(dumpfd, 40, SEEK_SET); // second place
  if (fwrite(&size, 4,1,dumpfd)!=1) goto error;

  fclose(dumpfd);
  return true;

  error:
  fclose(dumpfd);
  return false;
}

bool dump_wav(char *buf, int sz) {
  if (!inited) return false;
  if (fwrite(buf, sz,1,dumpfd)!=1) {
    fclose(dumpfd);
    return false;
  }
  size+=sz;
  return true;
}
/*
#include <printf.h>
#include <math.h>
#define NB_SAMPLE 2048
int main() {
  int n,m,res;
  int16 buf[NB_SAMPLE],
        val;
  res=init_dump_wav("out.wav");
  printf("init:%d\n",res);
  for (n=0,val=0;n<30;++n) {
    for (m=0;m<NB_SAMPLE;++m) {
      val+=5; val=val%628;
      buf[m]=(int16)(10000. * sin(val/100.));
    }
    res=dump_wav((char*)buf,NB_SAMPLE*2);
    if (!res) break;
  }
  printf("dump:%d\n",res);
  res=close_dump_wav();
  printf("close:%d\n",res);
}
*/
