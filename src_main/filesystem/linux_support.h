// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#ifndef LINUX_SUPPORT_H
#define LINUX_SUPPORT_H

#include <ctype.h>   // tolower()
#include <dirent.h>  // scandir()
#include <limits.h>  // PATH_MAX define
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    //strcmp, strcpy
#include <sys/stat.h>  // stat()
#include <unistd.h>

#define FILE_ATTRIBUTE_DIRECTORY S_IFDIR

typedef struct {
  // public data
  int dwFileAttributes;
  char cFileName[PATH_MAX];  // the file name returned from the call

  int numMatches;
  struct dirent **namelist;
} FIND_DATA;

#define WIN32_FIND_DATA FIND_DATA

#define SOURCE_MAX_PATH PATH_MAX

#define __stdcall

int FindFirstFile(char *findName, FIND_DATA *dat);
bool FindNextFile(int handle, FIND_DATA *dat);
bool FindClose(int handle);
const char *findFileInDirCaseInsensitive(const char *file);

#endif  // LINUX_SUPPORT_H
