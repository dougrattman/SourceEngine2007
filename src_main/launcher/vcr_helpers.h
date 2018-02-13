// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_LAUNCHER_VCR_HELPERS_H_
#define SOURCE_LAUNCHER_VCR_HELPERS_H_

#include <tuple>

#include "base/include/base_types.h"
#include "base/include/windows/windows_light.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"

class VCRHelpers : public IVCRHelpers {
 public:
  void ErrorMessage(const ch *message) override {
    NOVCR(MessageBoxA(nullptr, message, "Awesome Launcher - VCR Error",
                      MB_OK | MB_ICONERROR));
  }

  void *GetMainWindow() override { return nullptr; }
};

std::tuple<VCRHelpers, u32> BootstrapVCRHelpers(
    const ICommandLine *command_line);

#endif  // !SOURCE_LAUNCHER_VCR_HELPERS_H_
