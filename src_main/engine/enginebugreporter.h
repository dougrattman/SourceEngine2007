// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ENGINEBUGREPORTER_H
#define ENGINEBUGREPORTER_H

namespace vgui {
class Panel;
};

the_interface IEngineBugReporter {
 public:
  typedef enum {
    BR_AUTOSELECT = 0,
    BR_PUBLIC,
    BR_INTERNAL,
  } BR_TYPE;

  virtual void Init(void) = 0;
  virtual void Shutdown(void) = 0;

  virtual void InstallBugReportingUI(vgui::Panel * parent, BR_TYPE type) = 0;
  virtual bool ShouldPause() const = 0;

  virtual bool IsVisible()
      const = 0;  //< true iff the bug panel is active and on screen right now
};

extern IEngineBugReporter *bugreporter;

#endif  // ENGINEBUGREPORTER_H
