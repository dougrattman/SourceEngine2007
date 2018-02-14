// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/include/valve_off.h"

#include "instance.h"

#ifdef OS_POSIX
ch *GetCommandLine();
#endif

#include "tier0/include/threadtools.h"
#include "tier0/include/valve_on.h"

#include "resource.h"

namespace {
struct DialogInitInfo {
  const ch *file_name;
  i32 line;
  const ch *expression;
};

struct AssertDisable {
  ch file_name[_MAX_PATH];

  // If these are not -1, then this CAssertDisable only disables asserts on
  // lines between these values (inclusive).
  i32 line_min, line_max;

  // Decremented each time we hit this assert and ignore it, until it's 0.
  // Then the CAssertDisable is removed.
  // If this is -1, then we always ignore this assert.
  i32 ignore_times;

  AssertDisable *next;
};

bool g_bAssertsEnabled{true};
AssertDisable *g_pAssertDisables{nullptr};

i32 g_iLastLineRange{5};
i32 g_nLastIgnoreNumTimes{1};
i32 g_VXConsoleAssertReturnValue{-1};

// Set to true if they want to break in the debugger.
bool g_bBreak{false};

DialogInitInfo g_Info;

HWND g_hBestParentWindow;

inline bool IsDebugBreakEnabled() {
  return strstr(Plat_GetCommandLine(), "-debugbreak") != nullptr;
}

inline bool AreAssertsDisabled() {
  return strstr(Plat_GetCommandLine(), "-noassert") != nullptr;
}

bool AreAssertsEnabledInFileLine(const ch *file_name, i32 line) {
  AssertDisable **prev{&g_pAssertDisables};
  AssertDisable *next;

  for (AssertDisable *it{g_pAssertDisables}; it; it = next) {
    next = it->next;

    if (stricmp(file_name, it->file_name) == 0) {
      // Are asserts disabled in the whole file?
      bool are_asserts_enabled{true};
      if (it->line_min == -1 && it->line_max == -1) are_asserts_enabled = false;

      // Are asserts disabled on the specified line?
      if (line >= it->line_min && line <= it->line_max)
        are_asserts_enabled = false;

      if (!are_asserts_enabled) {
        // If this assert is only disabled for the next N times, then
        // countdown..
        if (it->ignore_times > 0) {
          --it->ignore_times;
          if (it->ignore_times == 0) {
            // Remove this one from the list.
            *prev = next;
            delete it;
            continue;
          }
        }

        return false;
      }
    }

    prev = &it->next;
  }

  return true;
}

AssertDisable *CreateNewAssertDisable(const ch *pFilename) {
  AssertDisable *assert_disable{new AssertDisable};
  assert_disable->next = g_pAssertDisables;
  g_pAssertDisables = assert_disable;

  assert_disable->line_min = assert_disable->line_max = -1;
  assert_disable->ignore_times = -1;

  strncpy(assert_disable->file_name, g_Info.file_name,
          ARRAYSIZE(assert_disable->file_name) - 1);
  assert_disable->file_name[ARRAYSIZE(assert_disable->file_name) - 1] = 0;

  return assert_disable;
}

inline void IgnoreAssertsInCurrentFile() {
  CreateNewAssertDisable(g_Info.file_name);
}

AssertDisable *IgnoreAssertsNearby(i32 range) {
  AssertDisable *assert_disable = CreateNewAssertDisable(g_Info.file_name);
  assert_disable->line_min = g_Info.line - range;
  assert_disable->line_max = g_Info.line + range;
  return assert_disable;
}

#ifdef OS_WIN
INT_PTR CALLBACK AssertDialogProc(HWND window,        // handle to dialog box
                                  UINT message,       // message
                                  WPARAM wide_param,  // first message parameter
                                  LPARAM low_param  // second message parameter
) {
  switch (message) {
    case WM_INITDIALOG: {
      SetDlgItemText(window, IDC_ASSERT_MSG_CTRL, g_Info.expression);
      SetDlgItemText(window, IDC_FILENAME_CONTROL, g_Info.file_name);

      SetDlgItemInt(window, IDC_LINE_CONTROL, g_Info.line, false);
      SetDlgItemInt(window, IDC_IGNORE_NUMLINES, g_iLastLineRange, false);
      SetDlgItemInt(window, IDC_IGNORE_NUMTIMES, g_nLastIgnoreNumTimes, false);

      // Center the dialog.
      RECT dialog_rect, desktop_rect;
      GetWindowRect(window, &dialog_rect);
      GetWindowRect(GetDesktopWindow(), &desktop_rect);
      SetWindowPos(window, HWND_TOP,
                   ((desktop_rect.right - desktop_rect.left) -
                    (dialog_rect.right - dialog_rect.left)) /
                       2,
                   ((desktop_rect.bottom - desktop_rect.top) -
                    (dialog_rect.bottom - dialog_rect.top)) /
                       2,
                   0, 0, SWP_NOSIZE);
    }
      return TRUE;

    case WM_COMMAND: {
      switch (LOWORD(wide_param)) {
        case IDC_IGNORE_FILE: {
          IgnoreAssertsInCurrentFile();
          EndDialog(window, 0);
          return TRUE;
        }

          // Ignore this assert N times.
        case IDC_IGNORE_THIS: {
          BOOL is_translated{FALSE};
          UINT value{GetDlgItemInt(window, IDC_IGNORE_NUMTIMES, &is_translated,
                                   FALSE)};
          if (is_translated && value > 1) {
            AssertDisable *assert_disable{IgnoreAssertsNearby(0)};
            assert_disable->ignore_times = value - 1;
            g_nLastIgnoreNumTimes = value;
          }

          EndDialog(window, 0);
          return TRUE;
        }

          // Always ignore this assert.
        case IDC_IGNORE_ALWAYS: {
          IgnoreAssertsNearby(0);
          EndDialog(window, 0);
          return TRUE;
        }

        case IDC_IGNORE_NEARBY: {
          BOOL is_translated{FALSE};
          UINT value{GetDlgItemInt(window, IDC_IGNORE_NUMLINES, &is_translated,
                                   FALSE)};
          if (!is_translated || value < 1) return TRUE;

          IgnoreAssertsNearby(value);
          EndDialog(window, 0);
          return TRUE;
        }

        case IDC_IGNORE_ALL: {
          g_bAssertsEnabled = false;
          EndDialog(window, 0);
          return TRUE;
        }

        case IDC_BREAK: {
          g_bBreak = true;
          EndDialog(window, 0);
          return TRUE;
        }
      }

      case WM_KEYDOWN: {
        // Escape?
        if (wide_param == 2) {
          // Ignore this assert.
          EndDialog(window, 0);
          return TRUE;
        }
      }
    }
      return TRUE;
  }

  return FALSE;
}

BOOL CALLBACK ParentWindowEnumProc(
    HWND window,      // handle to parent window
    LPARAM low_param  // application-defined value
) {
  if (IsWindowVisible(window)) {
    DWORD pid;
    GetWindowThreadProcessId(window, &pid);
    if (pid == (DWORD)low_param) {
      g_hBestParentWindow = window;
      return FALSE;  // Don't iterate any more.
    }
  }
  return TRUE;
}

HWND FindLikelyParentWindow() {
  // Enumerate top-level windows and take the first visible one with our
  // processID.
  g_hBestParentWindow = nullptr;
  EnumWindows(ParentWindowEnumProc, GetCurrentProcessId());
  return g_hBestParentWindow;
}
#endif
}  // namespace

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

DBG_INTERFACE bool DoNewAssertDialog(const ch *pFilename, i32 line,
                                     const ch *pExpression) {
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
  g_Info.file_name = pFilename;
  g_Info.line = line;
  g_Info.expression = pExpression;

  g_bBreak = false;

#ifdef OS_WIN
  if (!ThreadInMainThread()) {
    i32 result = MessageBox(nullptr, pExpression, "Assertion Failed",
                            MB_SYSTEMMODAL | MB_CANCELTRYCONTINUE);

    if (result == IDCANCEL) {
      IgnoreAssertsNearby(0);
    } else if (result == IDCONTINUE) {
      g_bBreak = true;
    }
  } else {
    HWND hParentWindow = FindLikelyParentWindow();

    DialogBox(global_tier0_instance, MAKEINTRESOURCE(IDD_ASSERT_DIALOG),
              hParentWindow, AssertDialogProc);
  }
#elif OS_POSIX
  fprintf(stderr, "%s %i %s", pFilename, line, pExpression);
#endif

  return g_bBreak;
}
