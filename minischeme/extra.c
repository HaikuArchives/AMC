// Demo of C interface for MiniScheme
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef __cplusplus
typedef char bool;
#endif
#include "miniscm.h"

void wbfun(pointer len,pointer txt) {
  printf("wbfun: len=%d txt=%s\n",int_value(len),string_value(txt));
}
pointer look(pointer a) {
  printf("look: %d\n",int_value(a));
  return mk_integer(int_value(a));
}
pointer happy(pointer a) {
  printf("happy: %d\n",int_value(a));
  return cons(mk_integer(int_value(a)),nil_pointer());
} 

pointer list_to_undef(pointer a) {
  char *dispatch;
  if (is_symbol(car(a))) {
    dispatch=sym_name(car(a));
    if (!strcmp(dispatch,"wbfun")) {
      wbfun(cadr(a),caddr(a));
      return mk_extra();
    }
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
    if (!strcmp(dispatch,"look")) return look(cadr(a));
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
    if (!strcmp(dispatch,"happy"))
      res=happy(cadr(a));
    else
      return Error_1("list>list: unknown dispatch type:",car(a));
  }
  else
    return Error_1("list>list: no symbol:",car(a));
  return res;
}
