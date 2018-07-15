// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ENGINEBUGREPORTER_H
#define ENGINEBUGREPORTER_H

namespace vgui {
class Panel;
};

the_interface IEngineBugReporter {
 public:
  enum BR_TYPE {
    BR_AUTOSELECT = 0,
    BR_PUBLIC,
    BR_INTERNAL,
  };

  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  virtual void InstallBugReportingUI(vgui::Panel * parent, BR_TYPE type) = 0;
  virtual bool ShouldPause() const = 0;

  //< true iff the bug panel is active and on screen right now
  virtual bool IsVisible() const = 0;
};

extern IEngineBugReporter *bugreporter;

#endif  // ENGINEBUGREPORTER_H
