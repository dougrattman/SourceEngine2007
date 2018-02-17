// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef LAUNCHER_IRESLISTGENERATOR_H_
#define LAUNCHER_IRESLISTGENERATOR_H_

#include "base/include/base_types.h"

class IResListGenerator {
 public:
  virtual void Init(ch const *base_dir, ch const *game_dir) = 0;
  virtual void Shutdown() = 0;
  virtual bool IsActive() = 0;

  virtual void SetupCommandLine() = 0;
  virtual bool ShouldContinue() = 0;
};

extern IResListGenerator *reslistgenerator;

void SortResList(ch const *pchFileName, ch const *pchSearchPath);

#endif  // LAUNCHER_IRESLISTGENERATOR_H_
