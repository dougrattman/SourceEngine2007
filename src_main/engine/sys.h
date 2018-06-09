// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Non-portable functions.

#ifndef SOURCE_ENGINE_SYS_H_
#define SOURCE_ENGINE_SYS_H_

#include "base/include/base_types.h"

#include "sysexternal.h"

// system IO
void Sys_mkdir(const ch *path);
int Sys_CompareFileTime(long ft1, long ft2);
ch const *Sys_FindFirst(const ch *path, ch *basename, int namelength);
ch const *Sys_FindNext(ch *basename, int namelength);
void Sys_FindClose();

// Takes a path ID filter
ch const *Sys_FindFirstEx(const ch *pWildcard, const ch *pPathID, ch *basename,
                          int namelength);

void Sys_ShutdownMemory();
void Sys_InitMemory();

void Sys_LoadHLTVDLL();
void Sys_UnloadHLTVDLL();

void Sys_Sleep(int msec);
bool Sys_GetRegKeyValue(const ch *pszSubKey, const ch *pszElement,
                        ch *pszReturnString, int nReturnLength,
                        const ch *pszDefaultValue);
bool Sys_GetRegKeyValueInt(ch *pszSubKey, const ch *pszElement,
                           long *pulReturnValue, long dwDefaultValue);
bool Sys_SetRegKeyValue(const ch *pszSubKey, const ch *pszElement,
                        const ch *pszValue);

struct alignas(4) FileAssociationInfo {
  ch const *extension;
  ch const *command_to_issue;
};

bool Sys_CreateFileAssociations(usize count, FileAssociationInfo *list);
void Sys_TestSendKey(const ch *key);

#endif  // SOURCE_ENGINE_SYS_H_
