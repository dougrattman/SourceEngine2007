// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_DEDICATED_DEDICATED_STEAM_APPLICATION_H_
#define SOURCE_DEDICATED_DEDICATED_STEAM_APPLICATION_H_

#include "appframework/AppFramework.h"

// This class is a helper class used for steam-based applications.
// It loads up the file system in preparation for using it to load other
// required modules from steam.
//
// I couldn't use the one in appframework because the dedicated server
// inlines all the filesystem code.
class DedicatedSteamApplication : public CSteamApplication {
 public:
  DedicatedSteamApplication(CSteamAppSystemGroup *pAppSystemGroup)
      : CSteamApplication{pAppSystemGroup} {}

  bool Create() override;
};

#endif  // !SOURCE_DEDICATED_DEDICATED_STEAM_APPLICATION_H_
