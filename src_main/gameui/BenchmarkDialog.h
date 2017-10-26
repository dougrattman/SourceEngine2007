// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#ifndef BENCHMARKDIALOG_H
#define BENCHMARKDIALOG_H

#include "vgui_controls/Frame.h"

// Purpose: Benchmark launch dialog
class CBenchmarkDialog : public vgui::Frame {
  DECLARE_CLASS_SIMPLE(CBenchmarkDialog, vgui::Frame);

 public:
  CBenchmarkDialog(vgui::Panel *parent, const char *name);

 private:
  MESSAGE_FUNC(RunBenchmark, "RunBenchmark");
};

#endif  // BENCHMARKDIALOG_H
