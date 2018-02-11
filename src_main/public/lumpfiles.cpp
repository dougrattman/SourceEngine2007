// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "lumpfiles.h"

#include "bspfile.h"
#include "filesystem.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

// Purpose: Generate a lump file name for a given bsp & index
void GenerateLumpFileName(const char *bspfilename, char *lumpfilename,
                          int iBufferSize, int iIndex) {
  char lumppre[MAX_PATH];
  V_StripExtension(bspfilename, lumppre, MAX_PATH);
  Q_snprintf(lumpfilename, iBufferSize, "%s_l_%d.lmp", lumppre, iIndex);
}
