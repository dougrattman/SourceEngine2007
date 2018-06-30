// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Utilities for setting vproject settings.

#ifndef SOURCE_TIER2_VCONFIG_H_
#define SOURCE_TIER2_VCONFIG_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"

// The registry keys that vconfig uses to store the current vproject directory.
#define VPROJECT_REG_KEY \
  "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"

// For accessing the environment variables we store the current vproject in.
void SetVConfigRegistrySetting(const ch *pName, const ch *pValue,
                               bool bNotify = true);
bool GetVConfigRegistrySetting(const ch *pName, char *pReturn, int size);

#ifdef OS_WIN
bool RemoveObsoleteVConfigRegistrySetting(const ch *pValueName,
                                          ch *pOldValue = nullptr,
                                          int size = 0);
#endif

bool ConvertObsoleteVConfigRegistrySetting(const ch *pValueName);

#endif  // SOURCE_TIER2_VCONFIG_H_
