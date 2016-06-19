#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <unistd.h>
#include <SupportDefs.h>
#include "read-wav-data.h"

RawData RAW[raw_max];
bool raw_data_okay;
const int bufmax=100;

bool read_wav(const char *file,const int nr,const int sbm) {
  char word[20];
  int16 dum16;
  int32 dum32,
        size,
        half_size;
//  const int scale=0x10000/sbm;
  const int scale=0x4000/sbm;
  char *buf;
  FILE *src=fopen(file,"r");
  if (!src) {
    alert("could not open .wav file: %s",file); return false;
  }
  if (nr>=raw_max) {
    alert("wav file nr > %d",raw_max); return false;
  }
  if (
    fread(word,4,1,src)!=1 || strncmp(word,"RIFF",4) || 
    fread(&dum32, 4,1,src)!=1 ||                         // header size
    fread(word, 8,1,src)!=1 || strncmp(word,"WAVEfmt ",8) ||
    fread(&dum32, 4,1,src)!=1 || dum32!=16 ||            // chunk size  
    fread(&dum16, 2,1,src)!=1                            // format tag (1 = uncompressed PCM)
  ) goto error;

  if (dum16!=1) { alert("format = %d, should be 1",dum16); return false; }
  if (fread(&dum16, 2,1,src)!=1)                         // no of channels
    goto error;
  if (dum16!=1) { alert("nr channels = %d, should be 1",dum16); return false; }
  if (fread(&dum32, 4,1,src)!=1)                   // rate
    goto error;
  if (dum32!=44100) { alert("rate = %d, must be 44100",dum32); return false; }
  if (fread(&dum32, 4,1,src)!=1)                   // average bytes/sec
    goto error;
  if (dum32!=44100*2) { alert("byte/sec = %d, must be 2*44100",dum32); return false; }
  if (fread(&dum16, 2,1,src)!=1 ||                // block align
      fread(&dum16, 2,1,src)!=1)                  // bits per sample
    goto error;
  if (dum16!=16) {
    alert("bits per sample is %d, must be 16",dum16); return false;
  }
  if (fread(word, 4,1,src)!=1 || strncmp(word,"data",4))
    goto error;
  if (fread(&size, 4,1,src)!=1)           // sample length
    goto error;
  half_size=size/4;  // 22KHz/44KHz, sizeof(char)/sizeof(int16)
  RAW[nr].size=half_size;

  buf=(char*)malloc(size);
  if (!buf) {
    alert("could not allocate space to read %s",file);
    return false;
  }
  if (fread(buf,size,1,src)!=1)
    goto error;
  // printf("size=%d\n",size);
  RAW[nr].buf=new int16[half_size+1];
  for (int n=0;n<size;n+=4) {
    dum16=*reinterpret_cast<int16*>(buf+n);
    RAW[nr].buf[n/4]=dum16/scale;
  }
  free(buf);
  fclose(src);
  return true;

  error:
  alert("format error in %s",file); return false;
  fclose(src);
}

bool get_app_path(char *app_path,const char *app_file) {
  int lres=readlink(app_file,app_path,bufmax-1);
  if (lres<0)  // not a link
    strcpy(app_path,app_file);
  else
    app_path[lres]=0;
  char *p=strrchr(app_path,'/');
  if (p) *p=0;
  else {
    alert("bad app file: %s",app_file);
    return false;
  }
  return true;
}

bool fill_raw_data(const char *theAppFile,const int sbm) {
  int nr;
  DIR *dp;
  direct *dir;
  char *ext;
  char wav_file[bufmax],
       app_path[bufmax];
  if (!get_app_path(app_path,theAppFile))
    return false;
  if ((dp=opendir(app_path))==0) {
    alert("unknown wave file dir: %s",app_path);
    return false;
  }
  while ((dir=readdir(dp))!=0) {
    if (!dir->d_ino)
      continue;
    ext=strrchr(dir->d_name,'.');
    if (!ext || strcmp(ext,".wav")) continue;
    // printf("wave file: %s\n",dir->d_name);
    nr=atoi(dir->d_name);
    if (nr>0) {
      sprintf(wav_file,"%s/%s",app_path,dir->d_name);
      if (!read_wav(wav_file,nr,sbm)) { closedir(dp); return false; }
      RAW[nr].file_name=strdup(dir->d_name);
    }
  }
  closedir(dp);
  raw_data_okay=true;
  return true;
}
/* 
int main() {
  fill_raw_data();
}
*/
