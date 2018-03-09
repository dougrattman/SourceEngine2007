// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IGAMEUIFUNCS_H
#define IGAMEUIFUNCS_H

#include <tuple>

#include "base/include/base_types.h"
#include "modes.h"
#include "tier1/interface.h"
#include "vgui/keycode.h"

the_interface IGameUIFuncs {
 public:
  virtual bool IsKeyDown(const ch *key, bool &is_down) const = 0;
  virtual const ch *GetBindingForButtonCode(ButtonCode_t code) const = 0;
  virtual ButtonCode_t GetButtonCodeForBind(const ch *binding_name) const = 0;

  // [modes array, size].
  virtual std::tuple<vmode_t *, usize> GetVideoModes() const = 0;
  virtual void SetFriendsID(u32 friends_id, const ch *friends_name) = 0;
  // [width, height].
  virtual std::tuple<i32, i32> GetDesktopResolution() const = 0;
  virtual bool IsConnectedToVACSecureServer() const = 0;
};

#define VENGINE_GAMEUIFUNCS_VERSION "VENGINE_GAMEUIFUNCS_VERSION006"

#endif  // IGAMEUIFUNCS_H
