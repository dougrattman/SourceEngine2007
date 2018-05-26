// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASEAUTOCOMPLETEFILELIST_H
#define BASEAUTOCOMPLETEFILELIST_H

#include "tier1/convar.h"

// Purpose: Simple helper class for doing autocompletion of all files in a
// specific directory by extension
class CBaseAutoCompleteFileList {
 public:
  CBaseAutoCompleteFileList(const ch *cmdname, const ch *subdir,
                            const ch *extension) {
    m_pszCommandName = cmdname;
    m_pszSubDir = subdir;
    m_pszExtension = extension;
  }

  int AutoCompletionFunc(const ch *partial,
                         ch commands[COMMAND_COMPLETION_MAXITEMS]
                                      [COMMAND_COMPLETION_ITEM_LENGTH]);

 private:
  const ch *m_pszCommandName;
  const ch *m_pszSubDir;
  const ch *m_pszExtension;
};

#define DECLARE_AUTOCOMPLETION_FUNCTION(command, subdirectory, extension)      \
  static int g_##command##_CompletionFunc(                                     \
      const ch *partial, ch commands[COMMAND_COMPLETION_MAXITEMS]          \
                                        [COMMAND_COMPLETION_ITEM_LENGTH]) {    \
    static CBaseAutoCompleteFileList command##Complete(#command, subdirectory, \
                                                       #extension);            \
    return command##Complete.AutoCompletionFunc(partial, commands);            \
  }

#define AUTOCOMPLETION_FUNCTION(command) g_##command##_CompletionFunc

//-----------------------------------------------------------------------------
// Purpose: Utility to quicky generate a simple console command with file name
// autocompletion
//-----------------------------------------------------------------------------
#if !defined(_XBOX)
#define CON_COMMAND_AUTOCOMPLETEFILE(name, func, description, subdirectory, \
                                     extension)                             \
  DECLARE_AUTOCOMPLETION_FUNCTION(name, subdirectory, extension)            \
  static ConCommand name##_command(#name, func, description, 0,             \
                                   AUTOCOMPLETION_FUNCTION(name));

#else
#define CON_COMMAND_AUTOCOMPLETEFILE(name, func, description, subdirectory, \
                                     extension)                             \
  static ConCommand name##_command(#name, func, description);

#endif
#endif  // BASEAUTOCOMPLETEFILELIST_H
