// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef LUMPFILES_H
#define LUMPFILES_H

#include "base/include/base_types.h"

constexpr inline i32 kMaxLumpFilesCount{128};

void GenerateLumpFileName(const ch *bsp_file_name, ch *lump_file_name,
                          usize lump_file_name_size, i32 lump_index);

template <usize lump_file_name_size>
void GenerateLumpFileName(const ch *bsp_file_name,
                          ch (&lump_file_name)[lump_file_name_size],
                          i32 lump_index) {
  GenerateLumpFileName(bsp_file_name, lump_file_name, lump_file_name_size,
                       lump_index);
}

#endif  // LUMPFILES_H
