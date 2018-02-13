// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef LAUNCHER_SOURCE_APP_SYSTEM_GROUP_H_
#define LAUNCHER_SOURCE_APP_SYSTEM_GROUP_H_

#include "appframework/IAppSystemGroup.h"
#include "base/include/base_types.h"
#include "tier0/include/icommandline.h"

#include "file_system_access_logger.h"

class IEngineAPI;
class IHammer;

// Inner loop: initialize, shutdown main systems, load steam.
class SourceAppSystemGroup : public CSteamAppSystemGroup {
 public:
  SourceAppSystemGroup(const ch *base_directory, bool is_text_mode,
                       const ICommandLine *command_line,
                       FileSystemAccessLogger &file_system_access_logger)
      : base_directory_{base_directory},
        is_edit_mode_{false},
        is_text_mode_{is_text_mode},
        engine_api_{nullptr},
        hammer_{nullptr},
        command_line_{command_line},
        file_system_access_logger_{file_system_access_logger} {
    g_all_files_access_logger = &file_system_access_logger_;
  }

  // Methods of IApplication.
  bool Create() override;
  bool PreInit() override;
  int Main() override;
  void PostShutdown() override;
  void Destroy() override;

 private:
  const ch *base_directory_;
  bool is_edit_mode_;
  const bool is_text_mode_;
  const ICommandLine *command_line_;
  IEngineAPI *engine_api_;
  IHammer *hammer_;
  FileSystemAccessLogger &file_system_access_logger_;

  const ch *DetermineDefaultMod();
  const ch *DetermineDefaultGame();

  SourceAppSystemGroup(const SourceAppSystemGroup &g) = delete;
  SourceAppSystemGroup &operator=(const SourceAppSystemGroup &g) = delete;
};

#endif  // LAUNCHER_SOURCE_APP_SYSTEM_GROUP_H_
