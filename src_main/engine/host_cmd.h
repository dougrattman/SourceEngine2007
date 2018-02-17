// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(HOST_CMD_H)
#define HOST_CMD_H

#include "host_saverestore.h"
#include "savegame_version.h"
#include "tier1/convar.h"

// The launcher includes this file, too
#ifndef LAUNCHERONLY

void Host_Init(bool bIsDedicated);
void Host_Shutdown();
int Host_Frame(float time, int iState);
void Host_ShutdownServer();
bool Host_NewGame(char *mapName, bool loadGame, bool bBackgroundLevel,
                  const char *pszOldMap = NULL, const char *pszLandmark = NULL);
void Host_Changelevel(bool loadfromsavedgame, const char *mapname,
                      const char *start);
void Host_Version(void);

extern ConVar host_name;

extern int gHostSpawnCount;

#endif  // LAUNCHERONLY
#endif  // HOST_CMD_H
