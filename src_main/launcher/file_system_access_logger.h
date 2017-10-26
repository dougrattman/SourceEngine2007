// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef LAUNCHER_FILE_SYSTEM_ACCESS_LOGGER_H_
#define LAUNCHER_FILE_SYSTEM_ACCESS_LOGGER_H_

#include "base/include/base_types.h"

#include "tier0/icommandline.h"
#include "tier0/platform.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlstring.h"

#define ENGINE_RESLIST_FILE "engine.lst"

// create file to dump out to.
class FileSystemAccessLogger {
 public:
  FileSystemAccessLogger(const ch *base_directory,
                         const ICommandLine *command_line);
  void Init();
  void Shutdown();
  void LogFile(const ch *file_path, const ch *options);

 private:
  bool is_active_;
  const ch *base_directory_;
  ch current_directory_[MAX_PATH];
  const ICommandLine *command_line_;

  // persistent across restarts
  CUtlRBTree<CUtlString, int> logged_file_names_;
  CUtlString resource_lists_directory_;
  CUtlString path_to_game_directory_;

  static void LogAllFilesFunc(const ch *file_path, const ch *options);
  void LogToAllReslist(ch const *line);

  FileSystemAccessLogger(const FileSystemAccessLogger &l) = delete;
  FileSystemAccessLogger &operator=(const FileSystemAccessLogger &l) = delete;
};

extern FileSystemAccessLogger *g_all_files_access_logger;

#endif  // LAUNCHER_FILE_SYSTEM_ACCESS_LOGGER_H_
