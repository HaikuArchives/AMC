#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Application.h>
#include <Mime.h>
#include <NodeInfo.h>
#include <Roster.h>
#include <AppFileInfo.h>
#include <TypeConstants.h> 

#include "amc-scheme.h"
#include "miniscm.h"

BMessenger amc_application;
BList team_list;
team_id teamid;
app_info appinfo;
bool amc_found;

bool tst_alive_amc() {
  BMessage mes(ALIVE);
  if (B_OK==amc_application.SendMessage(&mes,(BHandler*)0,1000)) // delivery timeout 1 msec
    return true;
  return false;
} 

bool find_amc() {  // sets amc_application
  be_roster->GetAppList(&team_list);
  for (int i=0;i<team_list.CountItems();i++) {
    teamid=(team_id)team_list.ItemAt(i);
    be_roster->GetRunningAppInfo(teamid, &appinfo);
    if (strcmp(appinfo.ref.name, "amc")==0) {
      puts("OKAY! amc found");
      amc_application=BMessenger(0, teamid);
      amc_found=true;
      break;
    }
  }
  return amc_found;
}

pointer seld_notes() {
  amc_found=tst_alive_amc();
  if (!amc_found && !find_amc()) 
    return Error_0("STOP! amc not running");
  BMessage mes(STORE_SEL),
           reply;
  StoredNote *notes;
  Settings *settings;
  ssize_t bytes=0;
  int n,
      items=0;
  amc_application.SendMessage(&mes,&reply);
  puts("amc did reply");
  switch (reply.what) {
    case CONTENTS_SEL:
      reply.FindData(SELD_NOTES,B_OBJECT_TYPE,(const void**)(&notes),&bytes);
      items=bytes/sizeof(StoredNote);
      reply.FindData(SETTINGS,B_OBJECT_TYPE,(const void**)(&settings),&bytes);
      break;
    default: return Error_0("seld notes: unknown reply");
  } 
  pointer ptr,
          out=nil_pointer();
  for (n=0;n<items;++n) {
    ptr=cons(mk_integer(notes[n].lnr),
         cons(mk_integer(notes[n].snr),
          cons(mk_integer(notes[n].sign),
           cons(mk_integer(notes[n].dur),nil_pointer()))));
    out=cons(ptr,out);
  }
  ptr=cons(mk_integer(settings->meter),nil_pointer());
  ptr=cons(mk_symbol("meter"),ptr);
  ptr=cons(ptr,nil_pointer());
  return cons(ptr,out);
}

pointer send_notes(pointer a) {
  amc_found=tst_alive_amc();
  if (!amc_found && !find_amc()) 
    return Error_0("STOP! amc not running");
  static StoredNote buffer[stored_notes_max];
  int n,
      lst_note=0,
      list_len=list_length(a);
  pointer p;
  if (list_len>=stored_notes_max) return Error_0("send_notes: list too long");
  for (n=0,p=a;n<list_len;++n,p=cdr(p)) {
    buffer[n].lnr=int_value(list_ref(car(p),0));
    buffer[n].snr=int_value(list_ref(car(p),1));
    buffer[n].sign=int_value(list_ref(car(p),2));
    buffer[n].dur=int_value(list_ref(car(p),3));
    //printf("n=%d lnr=%d\n",n,buffer[n].lnr);
  }
  BMessage mes(SEND_NOTES);
  mes.AddData(SENT_NOTES,B_OBJECT_TYPE,buffer,n*sizeof(StoredNote));
  amc_application.SendMessage(&mes);
  return mk_extra();
}

pointer random_int(pointer v1,pointer v2) {
  int minv=int_value(v1),
      maxv=int_value(v2);
  return mk_integer(minv + rand() % (maxv - minv + 1));
}

pointer list_to_undef(pointer a) {
  char *dispatch;
  if (is_symbol(car(a))) {
    dispatch=sym_name(car(a));
    if (!strcmp(dispatch,"send-notes"))
      return send_notes(cadr(a));
    return Error_1("list>undef: unknown dispatch type:",car(a));
  }
  else
    return Error_1("list>undef: no symbol:",car(a));
}

pointer list_to_int(pointer a) {
  int res=0;
  char *dispatch;
  if (is_symbol(car(a))) {
    dispatch=sym_name(car(a));
    if (!strcmp(dispatch,"random"))
      return random_int(cadr(a),caddr(a));
    else
      return Error_1("list>int: unknown dispatch type:",car(a));
  }
  else
    return Error_1("list>int: no symbol:",car(a));
  return mk_integer(res);
}

pointer list_to_list(pointer a) {
  pointer res=nil_pointer();
  char *dispatch;
  if (is_symbol(car(a))) {
    dispatch=sym_name(car(a));
    if (!strcmp(dispatch,"get-selected-notes"))
      res=seld_notes();
    else
      return Error_1("list>list: unknown dispatch type:",car(a));
  }
  else
    return Error_1("list>list: no symbol:",car(a));
  return res;
}
