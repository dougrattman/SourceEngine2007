// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_KEY_TRANSLATION_H_
#define SOURCE_VGUI_KEY_TRANSLATION_H_

#ifdef _WIN32
#pragma once
#endif

#include "vgui/keycode.h"

// Convert from Windows scan codes to VGUI key codes.
vgui::KeyCode KeyCode_VirtualKeyToVGUI(int key);
int KeyCode_VGUIToVirtualKey(vgui::KeyCode keycode);

#endif  // SOURCE_VGUI_KEY_TRANSLATION_H_
