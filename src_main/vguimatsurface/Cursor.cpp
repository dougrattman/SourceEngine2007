// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Methods associated with the cursor.

#define OEMRESOURCE  // for OCR_* cursor junk
#include "base/include/windows/windows_light.h"

#include "FileSystem.h"
#include "cursor.h"
#include "inputsystem/iinputsystem.h"
#include "tier0/include/dbg.h"
#include "tier0/include/vcrmode.h"
#include "tier1/UtlDict.h"
#include "vguimatsurface.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

namespace {
static HCURSOR the_default_cursors[20];
static HCURSOR the_current_cursor{nullptr};
static bool is_cursor_locked{false};
static bool is_cursor_visible{true};
}  // namespace

// Initializes cursors
void InitCursors() {
  // load up all default cursors
  the_default_cursors[dc_none] = nullptr;
  the_default_cursors[dc_arrow] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_NORMAL));
  the_default_cursors[dc_ibeam] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_IBEAM));
  the_default_cursors[dc_hourglass] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_WAIT));
  the_default_cursors[dc_crosshair] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_CROSS));
  the_default_cursors[dc_up] = LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_UP));
  the_default_cursors[dc_sizenwse] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_SIZENWSE));
  the_default_cursors[dc_sizenesw] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_SIZENESW));
  the_default_cursors[dc_sizewe] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_SIZEWE));
  the_default_cursors[dc_sizens] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_SIZENS));
  the_default_cursors[dc_sizeall] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_SIZEALL));
  the_default_cursors[dc_no] = LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_NO));
  the_default_cursors[dc_hand] =
      LoadCursorW(nullptr, MAKEINTRESOURCEW(OCR_HAND));

  is_cursor_locked = false;
  is_cursor_visible = true;
  the_current_cursor = the_default_cursors[dc_arrow];
}

#define USER_CURSOR_MASK 0x80000000UL

// Purpose: Simple manager for user loaded windows cursors in vgui
class UserCursorManager {
 public:
  void Shutdown();
  vgui::HCursor CreateCursorFromFile(char const *curOrAniFile,
                                     char const *pPathID);
  bool LookupCursor(vgui::HCursor cursor, HCURSOR &handle);

 private:
  CUtlDict<HCURSOR, unsigned long> user_cursors_;
};

void UserCursorManager::Shutdown() {
  for (unsigned long i{user_cursors_.First()};
       i != user_cursors_.InvalidIndex(); i = user_cursors_.Next(i)) {
    ::DestroyCursor(user_cursors_[i]);
  }
  user_cursors_.RemoveAll();
}

vgui::HCursor UserCursorManager::CreateCursorFromFile(char const *curOrAniFile,
                                                      char const *pPathID) {
  char fn[SOURCE_MAX_PATH];

  strcpy_s(fn, curOrAniFile);
  Q_strlower(fn);
  Q_FixSlashes(fn);

  unsigned long cursor_idx{user_cursors_.Find(fn)};
  if (cursor_idx != user_cursors_.InvalidIndex()) {
    return cursor_idx | USER_CURSOR_MASK;
  }

  g_pFullFileSystem->GetLocalCopy(fn);

  char fullpath[SOURCE_MAX_PATH];
  g_pFullFileSystem->RelativePathToFullPath(fn, pPathID, fullpath,
                                            sizeof(fullpath));

  HCURSOR cursor{LoadCursorFromFileA(fullpath)};
  cursor_idx = user_cursors_.Insert(fn, cursor);

  return cursor_idx | USER_CURSOR_MASK;
}

bool UserCursorManager::LookupCursor(vgui::HCursor cursor, HCURSOR &handle) {
  if (!(cursor & USER_CURSOR_MASK)) {
    handle = nullptr;
    return false;
  }

  unsigned long cursor_idx{cursor & ~USER_CURSOR_MASK};
  if (!user_cursors_.IsValidIndex(cursor_idx)) {
    handle = nullptr;
    return false;
  }

  handle = user_cursors_[cursor_idx];
  return true;
}

namespace {
static UserCursorManager user_cursor_manager;
}

vgui::HCursor Cursor_CreateCursorFromFile(char const *curOrAniFile,
                                          char const *pPathID) {
  return user_cursor_manager.CreateCursorFromFile(curOrAniFile, pPathID);
}

void Cursor_ClearUserCursors() { user_cursor_manager.Shutdown(); }

// Selects a cursor
void CursorSelect(HCursor cursor) {
  if (is_cursor_locked) return;

  is_cursor_visible = true;
  switch (cursor) {
    case dc_user:
    case dc_none:
    case dc_blank:
      is_cursor_visible = false;
      break;

    case dc_arrow:
    case dc_waitarrow:
    case dc_ibeam:
    case dc_hourglass:
    case dc_crosshair:
    case dc_up:
    case dc_sizenwse:
    case dc_sizenesw:
    case dc_sizewe:
    case dc_sizens:
    case dc_sizeall:
    case dc_no:
    case dc_hand:
      the_current_cursor = the_default_cursors[cursor];
      break;

    default: {
      HCURSOR custom{nullptr};
      if (user_cursor_manager.LookupCursor(cursor, custom) &&
          custom != nullptr) {
        the_current_cursor = custom;
      } else {
        is_cursor_visible = false;
        Assert(0);
      }
    } break;
  }

  ActivateCurrentCursor();
}

// Activates the current cursor.
void ActivateCurrentCursor() {
  ::SetCursor(is_cursor_visible ? the_current_cursor : nullptr);
}

// Prevents vgui from changing the cursor.
void LockCursor(bool is_lock_cursor) {
  is_cursor_locked = is_lock_cursor;
  ActivateCurrentCursor();
}

// Unlocks the cursor state.
bool IsCursorLocked() { return is_cursor_locked; }

// Handles mouse movement.
void CursorSetPos(void *hwnd, int x, int y) {
  g_pInputSystem->SetCursorPosition(x, y);
}

void CursorGetPos(void *hwnd, int &x, int &y) {
  POINT pt;

  // Default implementation.
  VCRHook_GetCursorPos(&pt);
  VCRHook_ScreenToClient((HWND)hwnd, &pt);

  x = pt.x;
  y = pt.y;
}
