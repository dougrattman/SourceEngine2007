// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "vcr_helpers.h"

#include <cstdlib>

#include "base/include/windows/windows_errno_info.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"

std::tuple<VCRHelpers, source::windows::windows_errno_code> BootstrapVCRHelpers(
    const ICommandLine *command_line) {
  const char *vcr_file_name;
  const bool is_vcr_record =
      command_line->CheckParm("-vcrrecord", &vcr_file_name);
  const bool is_vcr_playback =
      command_line->CheckParm("-vcrplayback", &vcr_file_name);

  if (is_vcr_record && is_vcr_playback) {
    Error(
        "-vcrrecord/-vcrplayback: Should use only -vcrrecord or "
        "-vcrplayback.\n",
        vcr_file_name);
    return {VCRHelpers{},
            source::windows::win32_to_windows_errno_code(ERROR_BAD_ARGUMENTS)};
  }

  VCRHelpers vcr_helpers;

  if (is_vcr_record && !VCRStart(vcr_file_name, true, &vcr_helpers)) {
    Error("-vcrrecord: Can't open '%s' for writing.\n", vcr_file_name);
    return {vcr_helpers,
            source::windows::win32_to_windows_errno_code(ERROR_BAD_ARGUMENTS)};
  }

  if (is_vcr_playback && !VCRStart(vcr_file_name, false, &vcr_helpers)) {
    Error("-vcrplayback: Can't open '%s' for reading.\n", vcr_file_name);
    return {vcr_helpers,
            source::windows::win32_to_windows_errno_code(ERROR_BAD_ARGUMENTS)};
  }

  return {vcr_helpers, S_OK};
}
