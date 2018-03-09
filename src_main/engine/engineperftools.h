// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ENGINEPERFTOOLS_H
#define ENGINEPERFTOOLS_H

namespace vgui {
class Panel;
};

the_interface IEnginePerfTools {
 public:
  virtual void Init(void) = 0;
  virtual void Shutdown(void) = 0;

  virtual void InstallPerformanceToolsUI(vgui::Panel * parent) = 0;
  virtual bool ShouldPause() const = 0;
};

extern IEnginePerfTools *perftools;

#endif  // ENGINEPERFTOOLS_H
