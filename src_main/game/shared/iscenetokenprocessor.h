// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef ISCENETOKENPROCESSOR_H
#define ISCENETOKENPROCESSOR_H

#include "tier0/platform.h"

abstract_class ISceneTokenProcessor {
 public:
  virtual const char *CurrentToken(void) = 0;
  virtual bool GetToken(bool crossline) = 0;
  virtual bool TokenAvailable(void) = 0;
  virtual void Error(const char *fmt, ...) = 0;
};

#endif  // ISCENETOKENPROCESSOR_H
