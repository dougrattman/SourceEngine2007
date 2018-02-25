// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Provide a shared place for library functions to report progress %
// for display.

#ifndef SOURCE_TIER0_INCLUDE_PROGRESSBAR_H_
#define SOURCE_TIER0_INCLUDE_PROGRESSBAR_H_

#include "base/include/base_types.h"
#include "tier0/include/tier0_api.h"

SOURCE_TIER0_API void ReportProgress(ch const *job_name, i32 total_units_to_do,
                                     i32 n_units_completed);

using ProgressReportHandler_t = void (*)(ch const *, i32, i32);

// install your own handler. returns previous handler
SOURCE_TIER0_API ProgressReportHandler_t
InstallProgressReportHandler(ProgressReportHandler_t pfn);

#endif  // SOURCE_TIER0_INCLUDE_PROGRESSBAR_H_
