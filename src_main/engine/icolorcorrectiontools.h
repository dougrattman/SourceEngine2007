// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef ICOLORCORRECTIONTOOLS_H
#define ICOLORCORRECTIONTOOLS_H

namespace vgui {
class Panel;
};

class IColorOperation;

the_interface IColorCorrectionTools {
 public:
  virtual void Init(void) = 0;
  virtual void Shutdown(void) = 0;

  virtual void InstallColorCorrectionUI(vgui::Panel * parent) = 0;
  virtual bool ShouldPause() const = 0;

  virtual void GrabPreColorCorrectedFrame(int x, int y, int width,
                                          int height) = 0;

  virtual void UpdateColorCorrection() = 0;

  virtual void SetFinalOperation(IColorOperation * pOp) = 0;
};

extern IColorCorrectionTools *colorcorrectiontools;

#endif  // COLORCORRECTIONTOOLS_H
