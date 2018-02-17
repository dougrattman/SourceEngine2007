// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef FILESYSTEM_ENGINE_H
#define FILESYSTEM_ENGINE_H

class IFileSystem;
class CSysModule;

// Filesystem interface
extern IFileSystem *g_pFileSystem;

// Loads DLLs through the file system (used by steam)
CSysModule *FileSystem_LoadModule(const char *path);
void FileSystem_UnloadModule(CSysModule *pModule);

void FileSystem_SetWhitelistSpewFlags();

#endif  // FILESYSTEM_ENGINE_H
