// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ISCENETOKENPROCESSOR_H
#define ISCENETOKENPROCESSOR_H

#include "tier0/include/platform.h"

abstract_class ISceneTokenProcessor {
 public:
  virtual const char *CurrentToken(void) = 0;
  virtual bool GetToken(bool crossline) = 0;
  virtual bool TokenAvailable(void) = 0;
  virtual void Error(const char *fmt, ...) = 0;
};

#endif  // ISCENETOKENPROCESSOR_H
