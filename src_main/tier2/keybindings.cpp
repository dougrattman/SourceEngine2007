// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier2/keybindings.h"

#include <algorithm>

#include "inputsystem/iinputsystem.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"

#include "tier0/include/memdbgon.h"

// Set a key binding
bool CKeyBindings::SetBinding(ButtonCode_t code, const ch *binding) {
  if (code == BUTTON_CODE_INVALID || code == KEY_NONE) return false;

  // free old bindings
  // Exactly the same, don't re-bind and fragment memory
  if (!key_infos_[code].IsEmpty() && !_stricmp(key_infos_[code], binding)) {
    return true;
  }

  // allocate memory for new binding
  key_infos_[code] = binding;

  return true;
}

bool CKeyBindings::SetBinding(const ch *button_name, const ch *binding) {
  const ButtonCode_t code{g_pInputSystem->StringToButtonCode(button_name)};

  return SetBinding(code, binding);
}

bool CKeyBindings::Unbind(ButtonCode_t code) {
  const bool was_unbind{code != KEY_NONE && code != BUTTON_CODE_INVALID};

  if (was_unbind) key_infos_[code] = "";

  return was_unbind;
}

bool CKeyBindings::Unbind(const ch *button_name) {
  const ButtonCode_t code{g_pInputSystem->StringToButtonCode(button_name)};

  return Unbind(code);
}

void CKeyBindings::UnbindAll() {
  for (auto &ki : key_infos_) ki = "";
}

// Count number of lines of bindings we'll be writing
usize CKeyBindings::GetBindingCount() const {
  return static_cast<usize>(
      std::count_if(std::begin(key_infos_), std::end(key_infos_),
                    [](const CUtlString &ki) { return ki.Length() > 0; }));
}

// Writes lines containing "bind key value".
void CKeyBindings::WriteBindings(CUtlBuffer &buffer) {
  for (int i = 0; i < BUTTON_CODE_LAST; i++) {
    if (key_infos_[i].Length()) {
      const ch *button_code{
          g_pInputSystem->ButtonCodeToString((ButtonCode_t)i)};

      buffer.Printf("bind \"%s\" \"%s\"\n", button_code, key_infos_[i].Get());
    }
  }
}

// Returns the keyname to which a binding string is bound.  E.g., if
// TAB is bound to +use then searching for +use will return "TAB"
const ch *CKeyBindings::ButtonNameForBinding(const ch *binding) {
  Assert(binding[0] != '\0');

  const ch *the_bind = binding;
  if (binding[0] == '+') ++the_bind;

  for (int i = 0; i < BUTTON_CODE_LAST; i++) {
    if (!key_infos_[i].Length()) continue;

    if (key_infos_[i][0] == '+') {
      if (!_stricmp(&key_infos_[i][1], the_bind))
        return g_pInputSystem->ButtonCodeToString((ButtonCode_t)i);
    } else {
      if (!_stricmp(key_infos_[i], the_bind))
        return g_pInputSystem->ButtonCodeToString((ButtonCode_t)i);
    }
  }

  return nullptr;
}

const ch *CKeyBindings::GetBindingForButton(ButtonCode_t code) {
  return key_infos_[code].IsEmpty() ? nullptr : key_infos_[code];
}
