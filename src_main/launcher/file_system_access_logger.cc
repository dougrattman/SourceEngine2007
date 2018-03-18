// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "file_system_access_logger.h"

#include "base/include/windows/windows_light.h"
#include "filesystem.h"
#include "ireslistgenerator.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/platform.h"
#include "tier1/fmtstr.h"
#include "tier1/strtools.h"

#include "tier0/include/memdbgon.h"

#define ALL_RESLIST_FILE "all.lst"

FileSystemAccessLogger *g_all_files_access_logger = nullptr;

inline static bool AllLogLessFunc(CUtlString const &left,
                                  CUtlString const &right) {
  return CaselessStringLessThan(left.Get(), right.Get());
}

FileSystemAccessLogger::FileSystemAccessLogger(const ch *base_directory,
                                               const ICommandLine *command_line)
    : is_active_{false},
      base_directory_{base_directory},
      command_line_{command_line},
      logged_file_names_{0, 0, AllLogLessFunc} {
  MEM_ALLOC_CREDIT();
  current_directory_[0] = '\0';
  resource_lists_directory_ = "reslists";
}

void FileSystemAccessLogger::Init() {
  // Can't do this in edit mode
  if (command_line_->CheckParm("-edit") ||
      !command_line_->CheckParm("-makereslists")) {
    return;
  }

  is_active_ = true;

  ch const *pszDir = nullptr;
  if (command_line_->CheckParm("-reslistdir", &pszDir) && pszDir) {
    ch szDir[SOURCE_MAX_PATH];

    Q_strncpy(szDir, pszDir, SOURCE_ARRAYSIZE(szDir));
    Q_StripTrailingSlash(szDir);
    Q_strlower(szDir);
    Q_FixSlashes(szDir);

    if (Q_strlen(szDir) > 0) {
      resource_lists_directory_ = szDir;
    }
  }

  // game directory has not been established yet, must derive ourselves
  ch path[SOURCE_MAX_PATH];
  Q_snprintf(path, SOURCE_ARRAYSIZE(path), "%s/%s", base_directory_,
             command_line_->ParmValue("-game", "hl2"));
  Q_FixSlashes(path);
  Q_strlower(path);
  path_to_game_directory_ = path;

  // create file to dump out to
  ch szDir[SOURCE_MAX_PATH];
  V_snprintf(szDir, SOURCE_ARRAYSIZE(szDir), "%s\\%s",
             path_to_game_directory_.String(),
             resource_lists_directory_.String());
  g_pFullFileSystem->CreateDirHierarchy(szDir, "GAME");

  g_pFullFileSystem->AddLoggingFunc(&LogAllFilesFunc);

  if (!command_line_->FindParm("-startmap") &&
      !command_line_->FindParm("-startstage")) {
    logged_file_names_.RemoveAll();
    g_pFullFileSystem->RemoveFile(
        CFmtStr("%s\\%s\\%s", path_to_game_directory_.String(),
                resource_lists_directory_.String(), ALL_RESLIST_FILE),
        "GAME");
  }

  GetCurrentDirectoryA(SOURCE_ARRAYSIZE(current_directory_),
                       current_directory_);
  Q_strncat(current_directory_, "\\", SOURCE_ARRAYSIZE(current_directory_), 1);
  _strlwr_s(current_directory_);
}

void FileSystemAccessLogger::Shutdown() {
  if (!is_active_) return;

  is_active_ = false;

  if (command_line_->CheckParm("-makereslists")) {
    g_pFullFileSystem->RemoveLoggingFunc(&LogAllFilesFunc);
  }

  // Now load and sort all.lst
  SortResList(CFmtStr("%s\\%s\\%s", path_to_game_directory_.String(),
                      resource_lists_directory_.String(), ALL_RESLIST_FILE),
              "GAME");
  // Now load and sort engine.lst
  SortResList(CFmtStr("%s\\%s\\%s", path_to_game_directory_.String(),
                      resource_lists_directory_.String(), ENGINE_RESLIST_FILE),
              "GAME");

  logged_file_names_.Purge();
}

void FileSystemAccessLogger::LogToAllReslist(ch const *line) {
  // Open for append, write data, close.
  FileHandle_t fh = g_pFullFileSystem->Open(
      CFmtStr("%s\\%s\\%s", path_to_game_directory_.String(),
              resource_lists_directory_.String(), ALL_RESLIST_FILE),
      "at", "GAME");
  if (fh != FILESYSTEM_INVALID_HANDLE) {
    g_pFullFileSystem->Write("\"", 1, fh);
    g_pFullFileSystem->Write(line, Q_strlen(line), fh);
    g_pFullFileSystem->Write("\"\n", 2, fh);
    g_pFullFileSystem->Close(fh);
  }
}

void FileSystemAccessLogger::LogFile(const ch *fullPathFileName,
                                     const ch *options) {
  if (!is_active_) {
    Assert(0);
    return;
  }

  // write out to log file
  Assert(fullPathFileName[1] == ':');

  int idx = logged_file_names_.Find(fullPathFileName);
  if (idx != logged_file_names_.InvalidIndex()) {
    return;
  }

  logged_file_names_.Insert(fullPathFileName);

  // make it relative to our root directory
  const ch *relative = Q_stristr(fullPathFileName, base_directory_);
  if (relative) {
    relative += Q_strlen(base_directory_) + 1;

    ch rel[SOURCE_MAX_PATH];
    Q_strncpy(rel, relative, SOURCE_ARRAYSIZE(rel));
    Q_strlower(rel);
    Q_FixSlashes(rel);

    LogToAllReslist(rel);
  }
}

// Purpose: callback function from file system.
void FileSystemAccessLogger::LogAllFilesFunc(const ch *fullPathFileName,
                                             const ch *options) {
  Assert(g_all_files_access_logger);
  if (g_all_files_access_logger)
    g_all_files_access_logger->LogFile(fullPathFileName, options);
}
