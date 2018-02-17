// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// sys.h -- non-portable functions

#ifndef SYS_H
#define SYS_H

#include "sysexternal.h"

// system IO
void Sys_mkdir(const char *path);
int Sys_CompareFileTime(long ft1, long ft2);
char const *Sys_FindFirst(const char *path, char *basename, int namelength);
char const *Sys_FindNext(char *basename, int namelength);
void Sys_FindClose();

// Takes a path ID filter
char const *Sys_FindFirstEx(const char *pWildcard, const char *pPathID,
                            char *basename, int namelength);

void Sys_ShutdownMemory();
void Sys_InitMemory();

void Sys_LoadHLTVDLL();
void Sys_UnloadHLTVDLL();

void Sys_Sleep(int msec);
void Sys_GetRegKeyValue(const char *pszSubKey, const char *pszElement,
                        char *pszReturnString, int nReturnLength,
                        const char *pszDefaultValue);
void Sys_GetRegKeyValueInt(char *pszSubKey, const char *pszElement,
                           long *pulReturnValue, long dwDefaultValue);
void Sys_SetRegKeyValue(const char *pszSubKey, const char *pszElement,
                        const char *pszValue);

extern "C" void Sys_SetFPCW();
extern "C" void Sys_TruncateFPU();

struct FileAssociationInfo {
  char const *extension;
  char const *command_to_issue;
};

void Sys_CreateFileAssociations(int count, FileAssociationInfo *list);
void Sys_TestSendKey(const char *pKey);
void Sys_OutputDebugString(const char *msg);

#endif  // SYS_H
