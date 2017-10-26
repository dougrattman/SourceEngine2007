// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef USERID_H
#define USERID_H

#include "SteamCommon.h"

#define IDTYPE_WON 0
#define IDTYPE_STEAM 1
#define IDTYPE_VALVE 2
#define IDTYPE_HLTV 3

typedef struct USERID_s {
  int idtype;

  union {
    TSteamGlobalUserID steamid;
  } uid;
} USERID_t;

#endif  // USERID_H
