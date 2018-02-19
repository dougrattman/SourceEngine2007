// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VRADDLL_H
#define VRADDLL_H

#include "ilaunchabledll.h"
#include "ivraddll.h"

class CVRadDLL : public IVRadDLL, public ILaunchableDLL {
  // IVRadDLL overrides.
 public:
  virtual int main(int argc, char **argv);
  virtual bool Init(char const *pFilename);
  virtual void Release();
  virtual void GetBSPInfo(CBSPInfo *pInfo);
  virtual bool DoIncrementalLight(char const *pVMFFile);
  virtual bool Serialize();
  virtual float GetPercentComplete();
  virtual void Interrupt();
};

#endif  // VRADDLL_H
