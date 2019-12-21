#ifndef _CONDVAR_H_
#define _CONDVAR_H_

#include "sem.h"

struct cs1550_lock {
  struct cs1550_sem mutex;
  struct cs1550_sem next;
  int nextCount;
};

struct cs1550_condition {
  struct cs1550_lock *l;
  struct cs1550_sem condSem;
  int semCount;
};

#endif
