// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#if !defined(FILTERS_H)
#define FILTERS_H

#include "userid.h"

typedef struct {
  unsigned mask;
  unsigned compare;
  float banEndTime;  // 0 for permanent ban
  float banTime;
} ipfilter_t;

typedef struct {
  USERID_t userid;
  float banEndTime;
  float banTime;
} userfilter_t;

#define MAX_IPFILTERS 32768
#define MAX_USERFILTERS 32768

#endif  // FILTERS_H
