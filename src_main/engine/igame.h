// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IGAME_H
#define IGAME_H

#include <tuple>
#include "base/include/base_types.h"
#include "tier1/interface.h"

the_interface IGame {
 public:
  virtual ~IGame() {}

  virtual bool Init(void *instance) = 0;
  virtual bool Shutdown() = 0;

  virtual bool CreateGameWindow() = 0;
  virtual void DestroyGameWindow() = 0;

  // This is used in edit mode to specify a particular game window (created by
  // hammer)
  virtual void SetGameWindow(void *hWnd) = 0;

  // This is used in edit mode to override the default wnd proc associated w/
  // the game window specified in SetGameWindow.
  virtual bool InputAttachToGameWindow() = 0;
  virtual void InputDetachFromGameWindow() = 0;

  virtual void PlayStartupVideos() = 0;

  virtual void *GetMainWindow() const = 0;
  virtual void **GetMainWindowAddress() const = 0;
  // [width, height, refresh rate]
  virtual std::tuple<i32, i32, i32> GetDesktopInfo() const = 0;

  virtual void SetWindowXY(i32 x, i32 y) = 0;
  virtual void SetWindowSize(i32 w, i32 h) = 0;

  // [x, y, width, height].
  virtual std::tuple<i32, i32, i32, i32> GetWindowRect() const = 0;

  // Not Alt-Tabbed away
  virtual bool IsActiveApp() const = 0;

  virtual void DispatchAllStoredGameMessages() = 0;
};

extern IGame *game;

#endif  // IGAME_H
