// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ILAUNCHABLEDLL_H
#define ILAUNCHABLEDLL_H

#define LAUNCHABLE_DLL_INTERFACE_VERSION "launchable_dll_1"

// vmpi_service can use this to debug worker apps in-process,
// and some of the launchers (like texturecompile) use this.
class ILaunchableDLL {
 public:
  // All vrad.exe does is load the VRAD DLL and run this.
  virtual int main(int argc, char **argv) = 0;
};

#endif  // ILAUNCHABLEDLL_H
