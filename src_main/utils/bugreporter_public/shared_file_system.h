// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BUGREPORTER_SHARED_FILE_SYSTEM_H_
#define BUGREPORTER_SHARED_FILE_SYSTEM_H_

#include "public/filesystem.h"

IBaseFileSystem* GetSharedFileSystem();
IBaseFileSystem* SetSharedFileSystem(IBaseFileSystem* new_shared_file_system);

#endif  // !BUGREPORTER_SHARED_FILE_SYSTEM_H_
