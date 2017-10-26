// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/valve_off.h"

#if defined(_WIN32)
#include "winlite.h"
#elif _LINUX
char *GetCommandLine();
#endif

#include "tier0/threadtools.h"
#include "tier0/valve_on.h"

#include "resource.h"

struct CDialogInitInfo {
  const char *m_pFilename;
  int m_iLine;
  const char *m_pExpression;
};

struct CAssertDisable {
  char m_Filename[512];

  // If these are not -1, then this CAssertDisable only disables asserts on
  // lines between these values (inclusive).
  int m_LineMin;
  int m_LineMax;

  // Decremented each time we hit this assert and ignore it, until it's 0.
  // Then the CAssertDisable is removed.
  // If this is -1, then we always ignore this assert.
  int m_nIgnoreTimes;

  CAssertDisable *m_pNext;
};

extern HINSTANCE g_hTier0Instance;

static bool g_bAssertsEnabled = true;
static CAssertDisable *g_pAssertDisables = nullptr;

static int g_iLastLineRange = 5;
static int g_nLastIgnoreNumTimes = 1;
static int g_VXConsoleAssertReturnValue = -1;

// Set to true if they want to break in the debugger.
static bool g_bBreak = false;

static CDialogInitInfo g_Info;

// Internal functions.

static bool IsDebugBreakEnabled() {
  return strstr(Plat_GetCommandLine(), "-debugbreak") != nullptr;
}

static bool AreAssertsDisabled() {
  return strstr(Plat_GetCommandLine(), "-noassert") != nullptr;
}

static bool AreAssertsEnabledInFileLine(const char *pFilename, int iLine) {
  CAssertDisable **pPrev = &g_pAssertDisables;
  CAssertDisable *pNext;
  for (CAssertDisable *pCur = g_pAssertDisables; pCur; pCur = pNext) {
    pNext = pCur->m_pNext;

    if (stricmp(pFilename, pCur->m_Filename) == 0) {
      // Are asserts disabled in the whole file?
      bool bAssertsEnabled = true;
      if (pCur->m_LineMin == -1 && pCur->m_LineMax == -1)
        bAssertsEnabled = false;

      // Are asserts disabled on the specified line?
      if (iLine >= pCur->m_LineMin && iLine <= pCur->m_LineMax)
        bAssertsEnabled = false;

      if (!bAssertsEnabled) {
        // If this assert is only disabled for the next N times, then
        // countdown..
        if (pCur->m_nIgnoreTimes > 0) {
          --pCur->m_nIgnoreTimes;
          if (pCur->m_nIgnoreTimes == 0) {
            // Remove this one from the list.
            *pPrev = pNext;
            delete pCur;
            continue;
          }
        }

        return false;
      }
    }

    pPrev = &pCur->m_pNext;
  }

  return true;
}

CAssertDisable *CreateNewAssertDisable(const char *pFilename) {
  CAssertDisable *pDisable = new CAssertDisable;
  pDisable->m_pNext = g_pAssertDisables;
  g_pAssertDisables = pDisable;

  pDisable->m_LineMin = pDisable->m_LineMax = -1;
  pDisable->m_nIgnoreTimes = -1;

  strncpy(pDisable->m_Filename, g_Info.m_pFilename,
          sizeof(pDisable->m_Filename) - 1);
  pDisable->m_Filename[sizeof(pDisable->m_Filename) - 1] = 0;

  return pDisable;
}

void IgnoreAssertsInCurrentFile() {
  CreateNewAssertDisable(g_Info.m_pFilename);
}

CAssertDisable *IgnoreAssertsNearby(int nRange) {
  CAssertDisable *pDisable = CreateNewAssertDisable(g_Info.m_pFilename);
  pDisable->m_LineMin = g_Info.m_iLine - nRange;
  pDisable->m_LineMax = g_Info.m_iLine + nRange;
  return pDisable;
}

#if defined(_WIN32)
INT_PTR CALLBACK AssertDialogProc(HWND hDlg,      // handle to dialog box
                                  UINT uMsg,      // message
                                  WPARAM wParam,  // first message parameter
                                  LPARAM lParam   // second message parameter
) {
  switch (uMsg) {
    case WM_INITDIALOG: {
      SetDlgItemText(hDlg, IDC_ASSERT_MSG_CTRL, g_Info.m_pExpression);
      SetDlgItemText(hDlg, IDC_FILENAME_CONTROL, g_Info.m_pFilename);

      SetDlgItemInt(hDlg, IDC_LINE_CONTROL, g_Info.m_iLine, false);
      SetDlgItemInt(hDlg, IDC_IGNORE_NUMLINES, g_iLastLineRange, false);
      SetDlgItemInt(hDlg, IDC_IGNORE_NUMTIMES, g_nLastIgnoreNumTimes, false);

      // Center the dialog.
      RECT rcDlg, rcDesktop;
      GetWindowRect(hDlg, &rcDlg);
      GetWindowRect(GetDesktopWindow(), &rcDesktop);
      SetWindowPos(
          hDlg, HWND_TOP,
          ((rcDesktop.right - rcDesktop.left) - (rcDlg.right - rcDlg.left)) / 2,
          ((rcDesktop.bottom - rcDesktop.top) - (rcDlg.bottom - rcDlg.top)) / 2,
          0, 0, SWP_NOSIZE);
    }
      return TRUE;

    case WM_COMMAND: {
      switch (LOWORD(wParam)) {
        case IDC_IGNORE_FILE: {
          IgnoreAssertsInCurrentFile();
          EndDialog(hDlg, 0);
          return TRUE;
        }

        // Ignore this assert N times.
        case IDC_IGNORE_THIS: {
          BOOL bTranslated = false;
          UINT value =
              GetDlgItemInt(hDlg, IDC_IGNORE_NUMTIMES, &bTranslated, false);
          if (bTranslated && value > 1) {
            CAssertDisable *pDisable = IgnoreAssertsNearby(0);
            pDisable->m_nIgnoreTimes = value - 1;
            g_nLastIgnoreNumTimes = value;
          }

          EndDialog(hDlg, 0);
          return TRUE;
        }

        // Always ignore this assert.
        case IDC_IGNORE_ALWAYS: {
          IgnoreAssertsNearby(0);
          EndDialog(hDlg, 0);
          return TRUE;
        }

        case IDC_IGNORE_NEARBY: {
          BOOL bTranslated = false;
          UINT value =
              GetDlgItemInt(hDlg, IDC_IGNORE_NUMLINES, &bTranslated, false);
          if (!bTranslated || value < 1) return TRUE;

          IgnoreAssertsNearby(value);
          EndDialog(hDlg, 0);
          return TRUE;
        }

        case IDC_IGNORE_ALL: {
          g_bAssertsEnabled = false;
          EndDialog(hDlg, 0);
          return TRUE;
        }

        case IDC_BREAK: {
          g_bBreak = true;
          EndDialog(hDlg, 0);
          return TRUE;
        }
      }

      case WM_KEYDOWN: {
        // Escape?
        if (wParam == 2) {
          // Ignore this assert.
          EndDialog(hDlg, 0);
          return TRUE;
        }
      }
    }
      return TRUE;
  }

  return FALSE;
}

static HWND g_hBestParentWindow;

static BOOL CALLBACK ParentWindowEnumProc(
    HWND hWnd,     // handle to parent window
    LPARAM lParam  // application-defined value
) {
  if (IsWindowVisible(hWnd)) {
    DWORD procID;
    GetWindowThreadProcessId(hWnd, &procID);
    if (procID == (DWORD)lParam) {
      g_hBestParentWindow = hWnd;
      return FALSE;  // don't iterate any more.
    }
  }
  return TRUE;
}

static HWND FindLikelyParentWindow() {
  // Enumerate top-level windows and take the first visible one with our
  // processID.
  g_hBestParentWindow = nullptr;
  EnumWindows(ParentWindowEnumProc, GetCurrentProcessId());
  return g_hBestParentWindow;
}
#endif

DBG_INTERFACE bool ShouldUseNewAssertDialog() {
  static bool is_mpi_worker =
      strstr(Plat_GetCommandLine(), "-mpi_worker") != nullptr;
  if (is_mpi_worker) {
    return false;
  }

#ifdef DBGFLAG_ASSERTDLG
  return true;  // always show an assert dialog
#else
  return Plat_IsInDebugSession();  // only show an assert dialog if the process
                                   // is being debugged
#endif  // DBGFLAG_ASSERTDLG
}

DBG_INTERFACE bool DoNewAssertDialog(const char *pFilename, int line,
                                     const char *pExpression) {
  LOCAL_THREAD_LOCK();

  if (AreAssertsDisabled()) return false;

  // If they have the old mode enabled (always break immediately), then just
  // break right into the debugger like we used to do.
  if (IsDebugBreakEnabled()) return true;

  // Have ALL Asserts been disabled?
  if (!g_bAssertsEnabled) return false;

  // Has this specific Assert been disabled?
  if (!AreAssertsEnabledInFileLine(pFilename, line)) return false;

  // Now create the dialog.
  g_Info.m_pFilename = pFilename;
  g_Info.m_iLine = line;
  g_Info.m_pExpression = pExpression;

  g_bBreak = false;

#if defined(_WIN32)
  if (!ThreadInMainThread()) {
    int result = MessageBox(nullptr, pExpression, "Assertion Failed",
                            MB_SYSTEMMODAL | MB_CANCELTRYCONTINUE);

    if (result == IDCANCEL) {
      IgnoreAssertsNearby(0);
    } else if (result == IDCONTINUE) {
      g_bBreak = true;
    }
  } else {
    HWND hParentWindow = FindLikelyParentWindow();

    DialogBox(g_hTier0Instance, MAKEINTRESOURCE(IDD_ASSERT_DIALOG),
              hParentWindow, AssertDialogProc);
  }
#elif _LINUX
  fprintf(stderr, "%s %i %s", pFilename, line, pExpression);
#endif

  return g_bBreak;
}
