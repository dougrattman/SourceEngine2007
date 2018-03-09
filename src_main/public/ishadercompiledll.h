// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ISHADERCOMPILEDLL_H
#define ISHADERCOMPILEDLL_H

#include "tier1/interface.h"

#define SHADER_COMPILE_INTERFACE_VERSION "shadercompiledll_0"

// This is the DLL interface to ShaderCompile.
the_interface IShaderCompileDLL {
 public:
  // All vrad.exe does is load the VRAD DLL and run this.
  virtual int main(int argc, char **argv) = 0;
};

#endif  // ISHADERCOMPILEDLL_H
