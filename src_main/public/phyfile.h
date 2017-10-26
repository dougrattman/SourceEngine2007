// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef PHYFILE_H
#define PHYFILE_H

#include "datamap.h"

typedef struct phyheader_s {
  DECLARE_BYTESWAP_DATADESC();
  int size;
  int id;
  int solidCount;
  long checkSum;  // checksum of source .mdl file
} phyheader_t;

#endif  // PHYFILE_H
