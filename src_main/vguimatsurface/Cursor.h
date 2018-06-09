// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Methods associated with the cursor

#ifndef MATSURFACE_CURSOR_H
#define MATSURFACE_CURSOR_H

#include "vgui/Cursor.h"
#include "vguimatsurface/IMatSystemSurface.h"

// Initializes cursors.
void InitCursors();

// Selects a cursor.
void CursorSelect(vgui::HCursor hCursor);

// Activates the current cursor.
void ActivateCurrentCursor();

// Handles mouse movement.
void CursorSetPos(void *hwnd, int x, int y);
void CursorGetPos(void *hwnd, int &x, int &y);

// Prevents vgui from changing the cursor
void LockCursor(bool bEnable);

// Unlocks the cursor state.
bool IsCursorLocked();

// Loads a custom cursor file from the file system.
vgui::HCursor Cursor_CreateCursorFromFile(char const *curOrAniFile,
                                          char const *pPathID);

// Helper for shutting down cursors.
void Cursor_ClearUserCursors();

#endif  // MATSURFACE_CURSOR_H
