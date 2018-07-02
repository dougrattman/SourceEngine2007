// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Helper methods + classes for file access

#include "tier2/fileutils.h"

#include "filesystem.h"
#include "tier0/include/icommandline.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"

ConVar fs_convert{"fs_convert", "1", 0,
                  "Allow Xbox 360 files to be generated at runtime"};

// Builds a directory which is a subdirectory of the current mod
void GetModSubdirectory(const char *pSubDir, char *pBuf, usize nBufLen) {
  // Compute starting directory
  Assert(g_pFullFileSystem->GetSearchPath("MOD", false, nullptr, 0) < nBufLen);

  g_pFullFileSystem->GetSearchPath("MOD", false, pBuf, nBufLen);
  char *pSemi = strchr(pBuf, ';');
  if (pSemi) *pSemi = '\0';

  Q_StripTrailingSlash(pBuf);

  if (pSubDir) {
    strcat_s(pBuf, nBufLen, "\\");
    strcat_s(pBuf, nBufLen, pSubDir);
  }

  Q_FixSlashes(pBuf);
}

// Builds a directory which is a subdirectory of the current mod's *content*
void GetModContentSubdirectory(const char *pSubDir, char *pBuf, usize nBufLen) {
  GetModSubdirectory(pSubDir, pBuf, nBufLen);
  char *pGame = Q_stristr(pBuf, "\\game\\");
  if (pGame) {
    ptrdiff_t nDistToEnd = nBufLen - (pGame + 6 - pBuf);
    memmove(pGame + 3, pGame, nDistToEnd - 3);

    const char *pContent = "\\content\\";
    memcpy(pGame, pContent, 9 * sizeof(char));
  } else {
    Warning("Current mod isn't in a subdirectory of \"game\"!\n");
  }
}

// Purpose: Generates an Xbox 360 filename from a PC filename
char *CreateX360Filename(const char *pSourceName, char *pTargetName,
                         usize targetLen) {
  Q_StripExtension(pSourceName, pTargetName, targetLen);
  const usize idx{strlen(pTargetName)};

  strcat_s(pTargetName, targetLen, ".360");
  // restore extension
  strcat_s(pTargetName, targetLen, &pSourceName[idx]);

  return pTargetName;
}

// Purpose: Generates a PC filename from a possible 360 name.  Strips the .360.
// from filename.360.extension.  Filenames might have multiple '.', need to be
// careful and only consider the last true extension.  Complex filenames do
// occur:
// d:\foo\.\foo.dat
// d:\zip0.360.zip\foo.360.dat
// Returns source if no change needs to occur, othwerwise generates and returns
// target.
char *RestoreFilename(const char *pSourceName, char *pTargetName,
                      usize targetLen) {
  // find extension
  // scan backward for '.', but not past a seperator
  usize end = strlen(pSourceName) - 1;
  while (end > 0 && pSourceName[end] != '.' &&
         !(pSourceName[end] == '\\' || pSourceName[end] == '/')) {
    --end;
  }

  if (end >= 4 && pSourceName[end] == '.' &&
      !V_strncmp(pSourceName + end - 4, ".360", 4)) {
    // cull the .360, leave the trailing extension
    end -= 4;
    usize length = std::min(end + 1, targetLen);
    V_strncpy(pTargetName, pSourceName, length);
    V_strncat(pTargetName, pSourceName + end + 4, targetLen);

    return pTargetName;
  }

  // source filename is as expected
  return (char *)pSourceName;
}

// Generate an Xbox 360 file if it doesn't exist or is out of date. This
// function determines the source and target path and whether the file needs to
// be generated. The caller provides a callback function to do the actual
// creation of the 360 file. "pExtraData" is for the caller to pass the address
// of any data that the callback function may need to access. This function is
// ONLY to be called by caller's who expect to have 360 versions of their file.
int UpdateOrCreate(const char *pSourceName, char *pTargetName, usize targetLen,
                   const char *pPathID, CreateCallback_t pfnCreate, bool bForce,
                   void *pExtraData) {
  // Will re-activate later code after shipping, and pursue.
  // The data conversions are requiring a greater complexity, or are cross
  // dependent. New work needs to be done for a stable long term developer
  // friendly solution.

  if (pTargetName) {
    // caller could supply source as PC or 360 name, we want the PC filename
    char szFixedSourceName[SOURCE_MAX_PATH];
    pSourceName = RestoreFilename(pSourceName, szFixedSourceName,
                                  sizeof(szFixedSourceName));
    // caller wants us to provide 360 named version of source
    CreateX360Filename(pSourceName, pTargetName, targetLen);
  }

  // no conversion are performed by the game at runtime anymore...
  return UOC_NOT_CREATED;
}

// Returns the search path as a list of paths
void GetSearchPath(CUtlVector<CUtlString> &path, const char *pPathID) {
  int nMaxLen = g_pFullFileSystem->GetSearchPath(pPathID, false, nullptr, 0);
  char *pBuf = stack_alloc<ch>(nMaxLen);

  g_pFullFileSystem->GetSearchPath(pPathID, false, pBuf, nMaxLen);

  char *semi;
  while (nullptr != (semi = strchr(pBuf, ';'))) {
    *semi = '\0';

    path.AddToTail(pBuf);

    pBuf = semi + 1;
  }

  path.AddToTail(pBuf);
}

// Builds a list of all files under a directory with a particular extension
void AddFilesToList(CUtlVector<CUtlString> &list, const char *pDirectory,
                    const char *pPathID, const char *pExtension) {
  char search_file[SOURCE_MAX_PATH];
  sprintf_s(search_file, "%s\\*", pDirectory);

  const bool is_absolute = Q_IsAbsolutePath(pDirectory);

  // get the list of files
  FileFindHandle_t find_handle;
  const char *found_file =
      g_pFullFileSystem->FindFirstEx(search_file, pPathID, &find_handle);

  // add all the items
  CUtlVector<CUtlString> sub_dirs;
  for (; found_file; found_file = g_pFullFileSystem->FindNext(find_handle)) {
    char child_path[SOURCE_MAX_PATH];
    sprintf_s(child_path, "%s\\%s", pDirectory, found_file);

    if (g_pFullFileSystem->FindIsDirectory(find_handle)) {
      if (Q_strnicmp(found_file, ".", 2) && Q_strnicmp(found_file, "..", 3)) {
        sub_dirs.AddToTail(child_path);
      }
      continue;
    }

    // Check the extension matches
    if (_stricmp(Q_GetFileExtension(found_file), pExtension) != 0) continue;

    char full_path_buffer[SOURCE_MAX_PATH];
    size_t full_path_size{std::size(full_path_buffer)};
    char *full_path = full_path_buffer;

    if (!is_absolute) {
      g_pFullFileSystem->RelativePathToFullPath(
          child_path, pPathID, full_path_buffer, std::size(full_path_buffer));
    } else {
      full_path = child_path;
      full_path_size = std::size(child_path);
    }

    _strlwr_s(full_path, full_path_size);
    Q_FixSlashes(full_path);

    list.AddToTail(full_path);
  }

  g_pFullFileSystem->FindClose(find_handle);

  const int subdirs_count = sub_dirs.Count();
  for (int i = 0; i < subdirs_count; ++i) {
    AddFilesToList(list, sub_dirs[i], pPathID, pExtension);
  }
}
