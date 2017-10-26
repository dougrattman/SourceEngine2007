// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef VMODES_H
#define VMODES_H

struct viddef_t {
  unsigned int width;
  unsigned int height;
  int recalc_refdef;  // if non-zero, recalc vid-based stuff
  int bits;
};

#endif  // VMODES_H
