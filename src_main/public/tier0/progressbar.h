// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Provide a shared place for library functions to report progress %
// for display.

#ifndef SOURCE_TIER0_PROGRESSBAR_H_
#define SOURCE_TIER0_PROGRESSBAR_H_

#include "tier0/platform.h"

PLATFORM_INTERFACE void ReportProgress(char const *job_name,
                                       int total_units_to_do,
                                       int n_units_completed);

typedef void (*ProgressReportHandler_t)(char const *, int, int);

// install your own handler. returns previous handler
PLATFORM_INTERFACE ProgressReportHandler_t
InstallProgressReportHandler(ProgressReportHandler_t pfn);

#endif  // SOURCE_TIER0_PROGRESSBAR_H_
