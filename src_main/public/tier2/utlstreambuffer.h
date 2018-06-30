// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Serialization/unserialization buffer.

#ifndef SOURCE_TIER2_UTLSTREAMBUFFER_H_
#define SOURCE_TIER2_UTLSTREAMBUFFER_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"

// Command parsing..
class CUtlStreamBuffer : public CUtlBuffer {
  using BaseClass = CUtlBuffer;

 public:
  // See CUtlBuffer::BufferFlags_t for flags
  CUtlStreamBuffer();
  CUtlStreamBuffer(const ch *pFileName, const ch *pPath, int nFlags = 0,
                   bool bDelayOpen = false);
  ~CUtlStreamBuffer();

  // Open the file. normally done in constructor
  void Open(const ch *pFileName, const ch *pPath, int nFlags);

  // close the file. normally done in destructor
  void Close();

  // Is the file open?
  bool IsOpen() const;

 private:
  // error flags
  enum {
    FILE_OPEN_ERROR = MAX_ERROR_FLAG << 1,
  };

  // Overflow functions
  bool StreamPutOverflow(int nSize);
  bool StreamGetOverflow(int nSize);

  // Grow allocation size to fit requested size
  void GrowAllocatedSize(int nSize);

  // Reads bytes from the file; fixes up maxput if necessary and 0 terminates
  int ReadBytesFromFile(int nBytesToRead, int nReadOffset);

  FileHandle_t OpenFile(const ch *pFileName, const ch *pPath);

  FileHandle_t m_hFileHandle;

  ch *m_pFileName;
  ch *m_pPath;
};

#endif  // SOURCE_TIER2_UTLSTREAMBUFFER_H_
