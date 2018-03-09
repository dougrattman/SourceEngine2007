// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef HOST_PHONEHOME_H
#define HOST_PHONEHOME_H

#include "tier1/interface.h"

the_interface IPhoneHome {
 public:
  enum {
    PHONE_MSG_UNKNOWN = 0,
    PHONE_MSG_ENGINESTART,
    PHONE_MSG_ENGINEEND,
    PHONE_MSG_MAPSTART,
    PHONE_MSG_MAPEND
  };

  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void Message(uint8_t msgtype, char const *mapname) = 0;
  virtual bool IsExternalBuild() = 0;
};

extern IPhoneHome *phonehome;

#endif  // HOST_PHONEHOME_H
