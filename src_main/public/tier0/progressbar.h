// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Provide a shared place for library functions to report progress %
// for display.

#ifndef SOURCE_TIER0_PROGRESSBAR_H_
#define SOURCE_TIER0_PROGRESSBAR_H_

#include "base/include/base_types.h"

#include "tier0/include/platform.h"

PLATFORM_INTERFACE void ReportProgress(ch const *job_name,
                                       i32 total_units_to_do,
                                       i32 n_units_completed);

typedef void (*ProgressReportHandler_t)(ch const *, i32, i32);

// install your own handler. returns previous handler
PLATFORM_INTERFACE ProgressReportHandler_t
InstallProgressReportHandler(ProgressReportHandler_t pfn);

#endif  // SOURCE_TIER0_PROGRESSBAR_H_
