// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: VMT tool; main UI smarts class

#ifndef SOURCE_TOOLS_VMT_VMTTOOL_H_
#define SOURCE_TOOLS_VMT_VMTTOOL_H_

#include "base/include/compiler_specific.h"

class CDmeEditorTypeDictionary;

// Singleton interfaces
extern CDmeEditorTypeDictionary *g_pEditorTypeDict;

// Allows the doc to call back into the VMT editor tool
the_interface IVMTDocCallback {
 public:
  // Called by the doc when the data changes
  virtual void OnDocChanged(const char *pReason, int nNotifySource,
                            int nNotifyFlags) = 0;

  // Update the editor dict based on the current material parameters
  virtual void AddShaderParameter(const char *pParam, const char *pWidget,
                                  const char *pTextType) = 0;

  // Update the editor dict based on the current material parameters
  virtual void RemoveShaderParameter(const char *pParam) = 0;

  // Adds flags, tool parameters
  virtual void AddFlagParameter(const char *pParam) = 0;
  virtual void AddToolParameter(const char *pParam,
                                const char *pWidget = nullptr,
                                const char *pTextType = nullptr) = 0;
  virtual void RemoveAllFlagParameters() = 0;
  virtual void RemoveAllToolParameters() = 0;
};

#endif  // SOURCE_TOOLS_VMT_VMTTOOL_H_
