// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SND_SFX_H
#define SND_SFX_H

class CAudioSource;

class CSfxTable {
 public:
  CSfxTable();

  // gets sound name, possible decoracted with prefixes
  virtual const char *getname();
  // gets the filename, the part after the optional prefixes
  const char *GetFileName();
  FileNameHandle_t GetFileNameHandle();

  void SetNamePoolIndex(int index);
  bool IsPrecachedSound();
  void OnNameChanged(const char *pName);

  int m_namePoolIndex;
  CAudioSource *pSource;

  bool m_bUseErrorFilename : 1;
  bool m_bIsUISound : 1;
  bool m_bIsLateLoad : 1;
  bool m_bMixGroupsCached : 1;
  uint8_t m_mixGroupCount;
  // UNDONE: Use a fixed bit vec here?
  uint8_t m_mixGroupList[8];

 private:
  // Only set in debug mode so you can see the name.
  const char *m_pDebugName;
};

#endif  // SND_SFX_H
