// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: used by all .exe's that run under steam and out,
// so they can be launched indirectly by steam and launch steam themselves

#ifndef SOURCE_STEAMAPPSTARTUP_H_
#define SOURCE_STEAMAPPSTARTUP_H_

// Purpose: Call this first thing at startup. Works out if the app is a steam
// app that is being ran outside of steam, and if so, launches steam and tells
// it to run us as a steam app.
// When true, then exit, else continue with normal startup.
bool ShouldLaunchAppViaSteam(const char *command_line,
                             const char *steam_file_system_dll_name,
                             const char *stdio_file_system_dll_name);

#endif  // SOURCE_STEAMAPPSTARTUP_H_
