// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "lumpfiles.h"

#include "bspfile.h"
#include "filesystem.h"
#include "tier1/strtools.h"

#include "tier0/include/memdbgon.h"

// Purpose: Generate a lump file name for a given bsp & index
void GenerateLumpFileName(const ch *bsp_file_name, ch *lump_file_name,
                          usize lump_file_name_size, i32 iIndex) {
  ch lump_prefix[SOURCE_MAX_PATH];
  V_StripExtension(bsp_file_name, lump_prefix, SOURCE_MAX_PATH);
  sprintf_s(lump_file_name, lump_file_name_size, "%s_l_%d.lmp", lump_prefix,
            iIndex);
}
