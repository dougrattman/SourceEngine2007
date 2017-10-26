// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef IGAMEUIFUNCS_H
#define IGAMEUIFUNCS_H

#include "vgui/keycode.h"

abstract_class IGameUIFuncs {
 public:
  virtual bool IsKeyDown(const char *keyname, bool &isdown) = 0;
  virtual const char *GetBindingForButtonCode(ButtonCode_t code) = 0;
  virtual ButtonCode_t GetButtonCodeForBind(const char *pBind) = 0;
  virtual void GetVideoModes(struct vmode_s * *liststart, int *count) = 0;
  virtual void SetFriendsID(uint32_t friendsID, const char *friendsName) = 0;
  virtual void GetDesktopResolution(int &width, int &height) = 0;
  virtual bool IsConnectedToVACSecureServer() = 0;
};

#define VENGINE_GAMEUIFUNCS_VERSION "VENGINE_GAMEUIFUNCS_VERSION005"

#endif  // IGAMEUIFUNCS_H
