// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SHADERDLL_H
#define SHADERDLL_H

#include "base/include/compiler_specific.h"
#include "materialsystem/IShader.h"

class IShader;
class ICvar;

// The standard implementation of CShaderDLL
the_interface IShaderDLL {
 public:
  // Adds a shader to the list of shaders
  virtual void InsertShader(IShader * shader) = 0;
};

// Singleton interface
IShaderDLL *GetShaderDLL();

// Singleton interface for CVars
ICvar *GetCVar();

#endif  // SHADERDLL_H
