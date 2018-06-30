// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#ifndef SOURCE_TIER2_FILEUTILS_H_
#define SOURCE_TIER2_FILEUTILS_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "filesystem.h"
#include "tier0/include/platform.h"
#include "tier2/tier2.h"

// Builds a directory which is a subdirectory of the current mod.
void GetModSubdirectory(const ch *subdir, ch *buffer, usize buffer_size);

// Builds a directory which is a subdirectory of the current mod's *content*.
void GetModContentSubdirectory(const ch *subdir, ch *buffer, usize buffer_size);

// Builds a list of all files under a directory with a particular extension.
void AddFilesToList(CUtlVector<CUtlString> &list, const ch *dir, const ch *path,
                    const ch *ext);

// Returns the search path as a list of paths
void GetSearchPath(CUtlVector<CUtlString> &path, const ch *path_id);

// Generates a .360 file if it doesn't exist or is out of sync with the pc
// source file.
#define UOC_FAIL -1
#define UOC_NOT_CREATED 0
#define UOC_CREATED 1

using CreateCallback_t = bool (*)(const ch *source_name, const ch *target_name,
                                  const ch *path_id, void *extra);
int UpdateOrCreate(const ch *source_name, ch *target_name, usize targetLen,
                   const ch *path_id, CreateCallback_t create_callback,
                   bool is_force = false, void *extra = nullptr);
char *CreateX360Filename(const ch *source_name, ch *target_name,
                         usize target_size);

// Simple file classes.  File I/O mode (text/binary, read/write) is based upon
// the subclass chosen.  classes with the word Required on them abort with a
// message if the file can't be opened.  Destructores close the file handle, or
// it can be explicitly closed with the Close() method.

class CBaseFile {
 public:
  FileHandle_t m_FileHandle;

  CBaseFile() : m_FileHandle{FILESYSTEM_INVALID_HANDLE} {}
  ~CBaseFile() { Close(); }

  void Close() {
    if (m_FileHandle != FILESYSTEM_INVALID_HANDLE)
      g_pFullFileSystem->Close(m_FileHandle);

    m_FileHandle = FILESYSTEM_INVALID_HANDLE;
  }

  void Open(ch const *fname, ch const *modes) {
    Close();
    m_FileHandle = g_pFullFileSystem->Open(fname, modes);
  }

  char *ReadLine(ch *pOutput, int maxChars) {
    return g_pFullFileSystem->ReadLine(pOutput, maxChars, m_FileHandle);
  }

  int Read(void *pOutput, int size) {
    return g_pFullFileSystem->Read(pOutput, size, m_FileHandle);
  }

  void MustRead(void *pOutput, int size) {
    int ret = Read(pOutput, size);
    if (ret != size) Error("failed to read %d bytes\n");
  }

  int Write(void const *pInput, int size) {
    return g_pFullFileSystem->Write(pInput, size, m_FileHandle);
  }

  // {Get|Put}{Int|Float} read and write ints and floats from a file in x86
  // order, swapping on input for big-endian systems.
  void PutInt(int n) { Write(&n, sizeof(n)); }

  int GetInt() {
    int ret;
    MustRead(&ret, sizeof(ret));
    return ret;
  }

  f32 GetFloat() {
    f32 ret;
    MustRead(&ret, sizeof(ret));
    return ret;
  }
  void PutFloat(f32 f) { Write(&f, sizeof(f)); }

  bool IsOk() const {
    return m_FileHandle != FILESYSTEM_INVALID_HANDLE &&
           g_pFullFileSystem->IsOk(m_FileHandle);
  }
};

class COutputFile : public CBaseFile {
 public:
  void Open(ch const *pFname) { CBaseFile::Open(pFname, "wb"); }

  COutputFile(ch const *pFname) : CBaseFile() { Open(pFname); }
  COutputFile() : CBaseFile() {}
};

class COutputTextFile : public CBaseFile {
 public:
  void Open(ch const *pFname) { CBaseFile::Open(pFname, "w"); }

  COutputTextFile(ch const *pFname) : CBaseFile{} { Open(pFname); }
  COutputTextFile() : CBaseFile{} {}
};

class CInputFile : public CBaseFile {
 public:
  void Open(ch const *pFname) { CBaseFile::Open(pFname, "rb"); }

  CInputFile(ch const *pFname) : CBaseFile{} { Open(pFname); }
  CInputFile() : CBaseFile{} {}
};

class CInputTextFile : public CBaseFile {
 public:
  void Open(ch const *pFname) { CBaseFile::Open(pFname, "r"); }

  CInputTextFile(ch const *pFname) : CBaseFile{} { Open(pFname); }
  CInputTextFile() : CBaseFile{} {}
};

class CRequiredInputTextFile : public CBaseFile {
 public:
  void Open(ch const *pFname) {
    CBaseFile::Open(pFname, "r");

    if (!IsOk()) {
      Error("error opening required file %s\n", pFname);
    }
  }

  CRequiredInputTextFile(ch const *pFname) : CBaseFile() { Open(pFname); }
  CRequiredInputTextFile() : CBaseFile{} {}
};

class CRequiredInputFile : public CBaseFile {
 public:
  void Open(ch const *pFname) {
    CBaseFile::Open(pFname, "rb");
    if (!IsOk()) {
      Error("error opening required file %s\n", pFname);
    }
  }

  CRequiredInputFile(ch const *pFname) : CBaseFile{} { Open(pFname); }
  CRequiredInputFile() : CBaseFile{} {}
};

#endif  // SOURCE_TIER2_FILEUTILS_H_
