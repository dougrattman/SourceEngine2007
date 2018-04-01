// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Handles running the OS commands for map compilation.

#include "stdafx.h"

#include <afxtempl.h>

#include <direct.h>
#include <io.h>
#include <process.h>
#include "GameConfig.h"
#include "GlobalFunctions.h"
#include "Options.h"
#include "ProcessWnd.h"
#include "RunCommands.h"
#include "hammer.h"

#include "tier0/include/memdbgon.h"

static bool s_bRunsCommands = false;

bool IsRunningCommands() { return s_bRunsCommands; }

static char *pszDocPath, *pszDocName, *pszDocExt;

CProcessWnd procWnd;

void FixGameVars(char *pszSrc, char *pszDst, BOOL bUseQuotes) {
  // run through the parms list and substitute $variable strings for
  //  the real thing
  char *pSrc = pszSrc, *pDst = pszDst;
  BOOL bInQuote = FALSE;
  while (pSrc[0]) {
    if (pSrc[0] == '$')  // found a parm
    {
      if (pSrc[1] == '$')  // nope, it's a single symbol
      {
        *pDst++ = '$';
        ++pSrc;
      } else {
        // figure out which parm it is ..
        ++pSrc;

        if (!bInQuote && bUseQuotes) {
          // not in quote, and subbing a variable.. start quote
          *pDst++ = '\"';
          bInQuote = TRUE;
        }

        if (!_strnicmp(pSrc, "file", 4)) {
          pSrc += 4;
          strcpy(pDst, pszDocName);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "ext", 3)) {
          pSrc += 3;
          strcpy(pDst, pszDocExt);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "path", 4)) {
          pSrc += 4;
          strcpy(pDst, pszDocPath);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "exedir", 6)) {
          pSrc += 6;
          strcpy(pDst, g_pGameConfig->m_szGameExeDir);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "bspdir", 6)) {
          pSrc += 6;
          strcpy(pDst, g_pGameConfig->szBSPDir);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "bsp_exe", 7)) {
          pSrc += 7;
          strcpy(pDst, g_pGameConfig->szBSP);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "vis_exe", 7)) {
          pSrc += 7;
          strcpy(pDst, g_pGameConfig->szVIS);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "light_exe", 9)) {
          pSrc += 9;
          strcpy(pDst, g_pGameConfig->szLIGHT);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "game_exe", 8)) {
          pSrc += 8;
          strcpy(pDst, g_pGameConfig->szExecutable);
          pDst += strlen(pDst);
        } else if (!_strnicmp(pSrc, "gamedir", 7)) {
          pSrc += 7;
          strcpy(pDst, g_pGameConfig->m_szModDir);
          pDst += strlen(pDst);
        }
      }
    } else {
      if (*pSrc == ' ' && bInQuote) {
        bInQuote = FALSE;
        *pDst++ = '\"';  // close quotes
      }

      // just copy the char into the destination buffer
      *pDst++ = *pSrc++;
    }
  }

  if (bInQuote) {
    bInQuote = FALSE;
    *pDst++ = '\"';  // close quotes
  }

  pDst[0] = 0;
}

static void RemoveQuotes(char *pBuf) {
  if (pBuf[0] == '\"') strcpy(pBuf, pBuf + 1);
  if (pBuf[strlen(pBuf) - 1] == '\"') pBuf[strlen(pBuf) - 1] = 0;
}

const char *GetErrorString() {
  static char error[256];
  if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, GetLastError(), 0, error, SOURCE_ARRAYSIZE(error),
                     nullptr)) {
    return "N/A";
  }

  char *cr = strchr(error, '\r');  // get rid of \r\n
  if (cr) cr[0] = '\0';

  return error;
}

bool RunCommands(CCommandArray &Commands, LPCTSTR pszOrigDocName) {
  s_bRunsCommands = true;

  procWnd.GetReady();

  char current_dir[SOURCE_MAX_PATH];
  _getcwd(current_dir, sizeof(current_dir));

  // cut up document name into file and extension components.
  //  create two sets of buffers - one set with the long filename
  //  and one set with the 8.3 format.

  char szDocLongPath[SOURCE_MAX_PATH] = {0},
       szDocLongName[SOURCE_MAX_PATH] = {0},
       szDocLongExt[SOURCE_MAX_PATH] = {0};
  char szDocShortPath[SOURCE_MAX_PATH] = {0},
       szDocShortName[SOURCE_MAX_PATH] = {0},
       szDocShortExt[SOURCE_MAX_PATH] = {0};

  GetFullPathName(pszOrigDocName, SOURCE_MAX_PATH, szDocLongPath, nullptr);
  GetShortPathName(pszOrigDocName, szDocShortPath, SOURCE_MAX_PATH);

  // split them up
  char *p = strrchr(szDocLongPath, '.');
  if (p && strrchr(szDocLongPath, '\\') < p &&
      strrchr(szDocLongPath, '/') < p) {
    // got the extension
    strcpy(szDocLongExt, p + 1);
    p[0] = 0;
  }

  p = strrchr(szDocLongPath, '\\');
  if (!p) p = strrchr(szDocLongPath, '/');
  if (p) {
    // got the filepart
    strcpy(szDocLongName, p + 1);
    p[0] = 0;
  }

  // split the short part up
  p = strrchr(szDocShortPath, '.');
  if (p && strrchr(szDocShortPath, '\\') < p &&
      strrchr(szDocShortPath, '/') < p) {
    // got the extension
    strcpy(szDocShortExt, p + 1);
    p[0] = 0;
  }

  p = strrchr(szDocShortPath, '\\');
  if (!p) p = strrchr(szDocShortPath, '/');
  if (p) {
    // got the filepart
    strcpy(szDocShortName, p + 1);
    p[0] = 0;
  }

  int iSize = Commands.GetSize(), i = 0;
  char *ppParms[32];

  if (iSize != 0) {
    procWnd.Append(CString{"----- Run command(s) started -----\r\n"});
  }

  while (iSize--) {
    CCOMMAND &cmd = Commands[i++];

    // anything there?
    if ((!cmd.szRun[0] && !cmd.iSpecialCmd) || !cmd.bEnable) continue;

    // set name pointers for long filenames
    pszDocExt = szDocLongExt;
    pszDocName = szDocLongName;
    pszDocPath = szDocLongPath;

    char szNewParms[SOURCE_MAX_PATH * 5], szNewRun[SOURCE_MAX_PATH * 5];

    // HACK: force the spawnv call for launching the game
    if (!Q_stricmp(cmd.szRun, "$game_exe")) {
      cmd.bUseProcessWnd = FALSE;
    }

    FixGameVars(cmd.szRun, szNewRun, TRUE);
    FixGameVars(cmd.szParms, szNewParms, TRUE);

    CString cmd_desc;
    cmd_desc.Format("%d>Running %s %s\r\n", i, szNewRun, szNewParms);
    procWnd.Append(cmd_desc);

    // create a parameter list (not always required)
    if (!cmd.bUseProcessWnd || cmd.iSpecialCmd) {
      char *p = szNewParms;
      ppParms[0] = szNewRun;
      int iArg = 1;
      BOOL bDone = FALSE;
      while (p[0]) {
        ppParms[iArg++] = p;
        while (p[0]) {
          if (p[0] == ' ') {
            // found a space-separator
            p[0] = 0;

            p++;

            // skip remaining white space
            while (*p == ' ') p++;

            break;
          }

          // found the beginning of a quoted parameters
          if (p[0] == '\"') {
            while (1) {
              p++;
              if (p[0] == '\"') {
                // found the end
                if (p[1] == 0) bDone = TRUE;
                p[1] = 0;  // kick its ass
                p += 2;

                // skip remaining white space
                while (*p == ' ') p++;

                break;
              }
            }
            break;
          }

          // else advance p
          ++p;
        }

        if (!p[0] || bDone) break;  // done.
      }

      ppParms[iArg] = nullptr;

      if (cmd.iSpecialCmd) {
        BOOL bError = FALSE;
        LPCTSTR error = "";

        if (cmd.iSpecialCmd == CCCopyFile && iArg == 3) {
          RemoveQuotes(ppParms[1]);
          RemoveQuotes(ppParms[2]);

          // don't copy if we're already there
          if (_stricmp(ppParms[1], ppParms[2]) &&
              (!CopyFile(ppParms[1], ppParms[2], FALSE))) {
            bError = TRUE;
            error = GetErrorString();
          }
        } else if (cmd.iSpecialCmd == CCDelFile && iArg == 2) {
          RemoveQuotes(ppParms[1]);
          if (!DeleteFile(ppParms[1])) {
            bError = TRUE;
            error = GetErrorString();
          }
        } else if (cmd.iSpecialCmd == CCRenameFile && iArg == 3) {
          RemoveQuotes(ppParms[1]);
          RemoveQuotes(ppParms[2]);
          if (rename(ppParms[1], ppParms[2])) {
            bError = TRUE;
            error = strerror(errno);
          }
        } else if (cmd.iSpecialCmd == CCChangeDir && iArg == 2) {
          RemoveQuotes(ppParms[1]);
          if (mychdir(ppParms[1]) == -1) {
            bError = TRUE;
            error = strerror(errno);
          }
        }

        if (bError) {
          CString message;
          message.Format("%d>Run failed (\"%s\")\r\n", i, error);
          procWnd.Append(message);

          procWnd.SetForegroundWindow();
          message += "\r\nDo you want to continue?";

          if (AfxMessageBox(message, MB_YESNO) == IDNO) break;
        }
      } else {
        // Change to the game exe folder before spawning the engine.
        // This is necessary for Steam to find the correct Steam DLL (it
        // uses the current working directory to search).
        char szDir[SOURCE_MAX_PATH];
        strcpy(szDir, szNewRun);
        Q_StripFilename(szDir);

        mychdir(szDir);

        // YWB Force asynchronous operation so that engine doesn't hang on
        //  exit???  Seems to work.
        // spawnv doesn't like quotes
        RemoveQuotes(szNewRun);
        _spawnv(/*cmd.bNoWait ?*/ _P_NOWAIT, szNewRun,
                (const char *const *)ppParms);
      }
    } else {
      procWnd.Execute(szNewRun, szNewParms);
    }

    // check for existence?
    if (cmd.bEnsureCheck) {
      char file[SOURCE_MAX_PATH];
      FixGameVars(cmd.szEnsureFn, file, FALSE);

      if (GetFileAttributes(file) == INVALID_FILE_ATTRIBUTES) {
        procWnd.SetForegroundWindow();

        // not there!
        CString message;
        message.Format(
            "%d>Run failed (Output '%s' was not built).\nDo you want to "
            "continue?",
            i, file);
        if (AfxMessageBox(message, MB_YESNO) == IDNO) break;  // outta here
      }
    }
  }

  if (iSize != 0) {
    procWnd.Append(CString{"----- Run command(s) ended -----\r\n"});
  }

  mychdir(current_dir);

  s_bRunsCommands = false;

  return TRUE;
}
