// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_LAUNCHER_IRESOURCE_LISTING_WRITER_H_
#define SOURCE_LAUNCHER_IRESOURCE_LISTING_WRITER_H_

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"

src_interface IResourceListingWriter {
  virtual void Init(ch const *base_dir, ch const *game_dir) = 0;
  virtual void Shutdown() = 0;
  virtual bool IsActive() = 0;

  virtual void SetupCommandLine() = 0;
  virtual bool ShouldContinue() = 0;
};

IResourceListingWriter *ResourceListing();

void SortResList(ch const *pchFileName, ch const *pchSearchPath);

#endif  // SOURCE_LAUNCHER_IRESOURCE_LISTING_WRITER_H_
