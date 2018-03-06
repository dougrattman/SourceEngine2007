// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIAL_SYSTEM_SHADERAPIDX9_WMI_H_
#define MATERIAL_SYSTEM_SHADERAPIDX9_WMI_H_

#include <tuple>
#include "base/include/base_types.h"
#include "base/include/windows/windows_light.h"

// [bytes, error code]
std::tuple<u64, HRESULT> GetVidMemBytes([[maybe_unused]] u32 adapter_idx);

#endif  // MATERIAL_SYSTEM_SHADERAPIDX9_WMI_H_
