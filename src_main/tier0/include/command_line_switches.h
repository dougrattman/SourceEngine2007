// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_COMMAND_LINE_SWITCHES_H_
#define SOURCE_TIER0_INCLUDE_COMMAND_LINE_SWITCHES_H_

#include "base/include/base_types.h"

namespace source {
namespace tier0 {
namespace command_line_switches {
// Allows to override base directory.
constexpr const ch *baseDirectory = "-basedir";
// Run in text mode? (No graphics or sound).
constexpr const ch *textMode = "-textmode";
// Change process priority to low.
constexpr const ch *priority_low = "-low";
// Change process priority to high.
constexpr const ch *priority_high = "-high";
// Path to game directory to run.
constexpr const ch *game_path = "-game";
}  // namespace command_line_switches
}  // namespace tier0
}  // namespace source

#endif  // !SOURCE_TIER0_INCLUDE_COMMAND_LINE_SWITCHES_H_
