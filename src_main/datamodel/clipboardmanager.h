// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H

#include "tier1/utlvector.h"

class KeyValues;
class IClipboardCleanup;

// Clipboard.
class CClipboardManager {
 public:
  CClipboardManager();
  ~CClipboardManager();

  void EmptyClipboard(bool bClearWindowsClipboard);
  void SetClipboardData(CUtlVector<KeyValues *> &data,
                        IClipboardCleanup *pfnOptionalCleanuFunction);
  void AddToClipboardData(KeyValues *add);
  void GetClipboardData(CUtlVector<KeyValues *> &data);
  bool HasClipboardData() const;

 private:
  CUtlVector<KeyValues *> m_Data;
  IClipboardCleanup *m_pfnCleanup;
};

#endif  // CLIPBOARDMANAGER_H
