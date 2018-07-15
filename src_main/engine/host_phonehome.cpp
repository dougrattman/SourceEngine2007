// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "host_phonehome.h"

#include "build/include/build_config.h"

#include "tier0/include/icommandline.h"

#include "tier0/include/memdbgon.h"

// TODO(d.rattman): Rewrite this bullshit.
bool IsExternalBuild() {
  return (CommandLine()->FindParm("-publicbuild") ||
          !CommandLine()->FindParm("-internalbuild") &&
              !CommandLine()->CheckParm("-dev"));
}
