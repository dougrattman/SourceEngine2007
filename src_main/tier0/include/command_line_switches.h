// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_COMMAND_LINE_SWITCHES_H_
#define SOURCE_TIER0_INCLUDE_COMMAND_LINE_SWITCHES_H_

#include "base/include/base_types.h"

namespace source::tier0::command_line_switches {
// Allows to override base directory.
constexpr const ch *baseDirectory = "-basedir";
// Run in text mode? (No graphics or sound).
constexpr const ch *textMode = "-textmode";
// Change process priority to low.
constexpr const ch *kCpuPriorityLow = "-low";
// Change process priority to high.
constexpr const ch *kCpuPriorityHigh = "-high";
// Path to game directory to run.
constexpr const ch *kGamePath = "-game";
}  // namespace source::tier0::command_line_switches

#endif  // !SOURCE_TIER0_INCLUDE_COMMAND_LINE_SWITCHES_H_
