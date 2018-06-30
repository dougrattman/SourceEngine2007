// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER2_KEYBINDINGS_H_
#define SOURCE_TIER2_KEYBINDINGS_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "inputsystem/ButtonCode.h"
#include "tier1/utlstring.h"

class CUtlBuffer;

class CKeyBindings {
 public:
  bool SetBinding(ButtonCode_t code, const ch *binding);
  bool SetBinding(const ch *button_name, const ch *binding);

  bool Unbind(ButtonCode_t code);
  bool Unbind(const ch *button_name);
  void UnbindAll();

  usize GetBindingCount() const;
  void WriteBindings(CUtlBuffer &buf);
  const ch *ButtonNameForBinding(const ch *binding);
  const ch *GetBindingForButton(ButtonCode_t code);

 private:
  CUtlString key_infos_[BUTTON_CODE_LAST];
};

#endif  // SOURCE_TIER2_KEYBINDINGS_H_
