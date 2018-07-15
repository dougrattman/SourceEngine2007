// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(FILTERS_H)
#define FILTERS_H

#include "userid.h"

struct ipfilter_t {
  unsigned mask;
  unsigned compare;
  float banEndTime;  // 0 for permanent ban
  float banTime;
};

struct userfilter_t {
  USERID_t userid;
  float banEndTime;
  float banTime;
};

#define MAX_IPFILTERS 32768
#define MAX_USERFILTERS 32768

#endif  // FILTERS_H
