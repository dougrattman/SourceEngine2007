// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "baseautocompletefilelist.h"

#include "qlimits.h"
#include "sys.h"
#include "tier1/utlsymbol.h"

#include "tier0/include/memdbgon.h"

// Purpose: Fills in a list of commands based on specified subdirectory and
// extension into the format:
//  commandname subdir/filename.ext
//  commandname subdir/filename2.ext
// Returns number of files in list for autocompletion
int CBaseAutoCompleteFileList::AutoCompletionFunc(
    char const *partial, char commands[COMMAND_COMPLETION_MAXITEMS]
                                      [COMMAND_COMPLETION_ITEM_LENGTH]) {
  char const *command_name = m_pszCommandName;

  char *substring = (char *)partial;
  if (Q_strstr(partial, command_name)) {
    substring = (char *)partial + strlen(command_name) + 1;
  }

  // Search the directory structure.
  char search_path[MAX_QPATH];
  if (m_pszSubDir && m_pszSubDir[0] && Q_strcasecmp(m_pszSubDir, "NULL")) {
    Q_snprintf(search_path, std::size(search_path), "%s/*.%s", m_pszSubDir,
               m_pszExtension);
  } else {
    Q_snprintf(search_path, std::size(search_path), "*.%s", m_pszExtension);
  }

  CUtlSymbolTable entries{0, 0, true};
  CUtlVector<CUtlSymbol> symbols;
  const size_t substring_length{strlen(substring)};

  char const *file_name = Sys_FindFirst(search_path, nullptr, 0);
  while (file_name) {
    bool do_add = false;
    // Insert into lookup
    if (substring[0]) {
      if (!Q_strncasecmp(file_name, substring, substring_length)) {
        do_add = true;
      }
    } else {
      do_add = true;
    }

    if (do_add) {
      CUtlSymbol symbol = entries.AddString(file_name);

      int idx = symbols.Find(symbol);
      if (idx == symbols.InvalidIndex()) {
        symbols.AddToTail(symbol);
      }
    }

    file_name = Sys_FindNext(nullptr, 0);

    // Too many
    if (symbols.Count() >= COMMAND_COMPLETION_MAXITEMS) break;
  }

  Sys_FindClose();

  for (int i = 0; i < symbols.Count(); i++) {
    char const *filename = entries.String(symbols[i]);

    Q_snprintf(commands[i], std::size(commands[i]), "%s %s", command_name,
               filename);
    // Remove .dem
    commands[i][strlen(commands[i]) - 4] = 0;
  }

  return symbols.Count();
}
