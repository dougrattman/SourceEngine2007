// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: loads additional command line options from a config file

#include "FileSystem_Tools.h"
#include "KeyValues.h"
#include "tier1/strtools.h"
#include "tier1/utlstring.h"

// So we know whether or not we own argv's memory
static bool sFoundConfigArgs = false;

//-----------------------------------------------------------------------------
// Purpose: Parses arguments and adds them to argv and argc
//-----------------------------------------------------------------------------
static void AddArguments(int &argc, char **&argv, const char *str) {
  char **args = 0;
  int argCt = argc;

  size_t arg_list_size{strlen(str) + 1};
  char *argList = new char[arg_list_size];
  strcpy_s(argList, arg_list_size, str);

  // Parse the arguments out of the string
  char *context;
  char *token = strtok_s(argList, " ", &context);
  while (token) {
    ++argCt;
    token = strtok_s(nullptr, " ", &context);
  }

  // Make sure someting was actually found in the file
  if (argCt > argc) {
    sFoundConfigArgs = true;

    // Allocate a new array for argv
    args = new char *[argCt];

    // Copy original arguments, up to the last one
    int i;
    for (i = 0; i < argc - 1; ++i) {
      size_t argc_size{strlen(argv[i]) + 1};
      args[i] = new char[argc_size];
      strcpy_s(args[i], argc_size, argv[i]);
    }

    // copy new arguments
    strcpy_s(argList, arg_list_size, str);
    token = strtok_s(argList, " ", &context);
    for (; i < argCt - 1; ++i) {
      size_t argc_size{strlen(token) + 1};
      args[i] = new char[argc_size];
      strcpy_s(args[i], argc_size, token);
      token = strtok_s(nullptr, " ", &context);
    }

    // Copy the last original argument
    size_t argc_size{strlen(argv[argc - 1]) + 1};
    args[i] = new char[argc_size];
    strcpy_s(args[i], argc_size, argv[argc - 1]);

    argc = argCt;
    argv = args;
  }

  delete[] argList;
}

//-----------------------------------------------------------------------------
// Purpose: Loads additional commandline arguments from a config file for an
// app.
//			Filesystem must be initialized before calling this
// function.
// keyname: Name of the block containing the key/args pairs (ie map or model
// name) appname: Keyname for the commandline arguments to be loaded - typically
// the exe name.
//-----------------------------------------------------------------------------
void LoadCmdLineFromFile(int &argc, char **&argv, const char *keyname,
                         const char *appname) {
  sFoundConfigArgs = false;

  Assert(g_pFileSystem);
  if (!g_pFileSystem) return;

  // Load the cfg file, and find the keyname
  KeyValues *kv = new KeyValues("CommandLine");

  char filename[512];
  Q_snprintf(filename, sizeof(filename), "%s/cfg/commandline.cfg", gamedir);

  if (kv->LoadFromFile(g_pFileSystem, filename)) {
    // Load the commandline arguments for this app
    KeyValues *appKey = kv->FindKey(keyname);
    if (appKey) {
      const char *str = appKey->GetString(appname);
      Msg("Command Line found: %s\n", str);

      AddArguments(argc, argv, str);
    }
  }

  kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Cleans up any memory allocated for the new argv.  Pass in the app's
// argc and argv - this is safe even if no extra arguments were loaded.
//-----------------------------------------------------------------------------
void DeleteCmdLine(int argc, char **argv) {
  if (!sFoundConfigArgs) return;

  for (int i = 0; i < argc; ++i) {
    delete[] argv[i];
  }
  delete[] argv;
}
