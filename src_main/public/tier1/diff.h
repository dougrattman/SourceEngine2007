// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Serialization/unserialization buffer.

#ifndef SOURCE_TIER1_DIFF_H_
#define SOURCE_TIER1_DIFF_H_

#include "tier0/include/basetypes.h"

int FindDiffs(uint8_t const *NewBlock, uint8_t const *OldBlock, int NewSize,
              int OldSize, int &DiffListSize, uint8_t *Output, uint32_t OutSize);

int FindDiffsForLargeFiles(uint8_t const *NewBlock, uint8_t const *OldBlock,
                           int NewSize, int OldSize, int &DiffListSize,
                           uint8_t *Output, uint32_t OutSize, int hashsize = 65536);

void ApplyDiffs(uint8_t const *OldBlock, uint8_t const *DiffList, int OldSize,
                int DiffListSize, int &ResultListSize, uint8_t *Output,
                uint32_t OutSize);

int FindDiffsLowMemory(uint8_t const *NewBlock, uint8_t const *OldBlock,
                       int NewSize, int OldSize, int &DiffListSize,
                       uint8_t *Output, uint32_t OutSize);

#endif  // SOURCE_TIER1_DIFF_H_
