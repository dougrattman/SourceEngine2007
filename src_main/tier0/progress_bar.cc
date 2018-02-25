// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/include/progressbar.h"

#include <utility>

#include "tier0/include/platform.h"

namespace {
ProgressReportHandler_t progress_report_handler;
}  // namespace

SOURCE_TIER0_API void ReportProgress(ch const *job_name,
                                       i32 total_units_to_do,
                                       i32 n_units_completed) {
  if (progress_report_handler)
    (*progress_report_handler)(job_name, total_units_to_do, n_units_completed);
}

SOURCE_TIER0_API ProgressReportHandler_t
InstallProgressReportHandler(ProgressReportHandler_t handler) {
  return std::exchange(progress_report_handler, handler);
}
