// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_COMMAND_LINE_SWITCHES_H_
#define SOURCE_TIER0_COMMAND_LINE_SWITCHES_H_

namespace source {
namespace tier0 {
namespace command_line_switches {
// Allows to override base directory.
constexpr const char *baseDirectory = "-basedir";
// Run in text mode? (No graphics or sound).
constexpr const char *textMode = "-textmode";
// Change process priority to low.
constexpr const char *priority_low = "-low";
// Change process priority to high.
constexpr const char *priority_high = "-high";
// Path to game directory to run.
constexpr const char *game_path = "-game";
}  // namespace command_line_switches
}  // namespace tier0
}  // namespace source

#endif  // !SOURCE_TIER0_COMMAND_LINE_SWITCHES_H_
