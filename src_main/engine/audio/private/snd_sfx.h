// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SND_SFX_H
#define SND_SFX_H

#include "base/include/base_types.h"

class CAudioSource;

class CSfxTable {
 public:
  CSfxTable();

  // Gets sound name, possible decoracted with prefixes.
  virtual const ch *getname();
  // Gets the filename, the part after the optional prefixes.
  const ch *GetFileName();
  FileNameHandle_t GetFileNameHandle();

  void SetNamePoolIndex(int index);
  bool IsPrecachedSound();
  void OnNameChanged(const ch *name);

  int m_namePoolIndex;
  CAudioSource *pSource;

  bool m_bUseErrorFilename : 1;
  bool m_bIsUISound : 1;
  bool m_bIsLateLoad : 1;
  bool m_bMixGroupsCached : 1;
  u8 m_mixGroupCount;
  // UNDONE: Use a fixed bit vec here?
  u8 m_mixGroupList[8];

 private:
  // Only set in debug mode so you can see the name.
  const ch *m_pDebugName;
};

#endif  // SND_SFX_H
