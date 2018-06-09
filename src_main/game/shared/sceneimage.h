// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_CHOREOOBJECTS_SCENEIMAGE_H_
#define SOURCE_CHOREOOBJECTS_SCENEIMAGE_H_

#include "base/include/macros.h"

the_interface ISceneTokenProcessor;

the_interface ISceneCompileStatus {
 public:
  virtual void UpdateStatus(char const *pchSceneName, bool bQuiet, int nIndex,
                            int nCount) = 0;
};

class CUtlBuffer;

the_interface ISceneImage {
 public:
  virtual bool CreateSceneImageFile(
      CUtlBuffer & targetBuffer, char const *pchModPath, bool bLittleEndian,
      bool bQuiet, ISceneCompileStatus *Status) = 0;
};

extern ISceneImage *g_pSceneImage;
extern ISceneTokenProcessor *tokenprocessor;

#endif  // SOURCE_CHOREOOBJECTS_SCENEIMAGE_H_