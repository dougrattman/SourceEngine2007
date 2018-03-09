// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "shared_file_system.h"

#include <utility>

namespace {
IBaseFileSystem* shared_file_system = nullptr;
}  // namespace

IBaseFileSystem* GetSharedFileSystem() { return shared_file_system; }

IBaseFileSystem* SetSharedFileSystem(IBaseFileSystem* new_shared_file_system) {
  return std::exchange(shared_file_system, new_shared_file_system);
}
