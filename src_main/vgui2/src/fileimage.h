// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_FILEIMAGE_H_
#define SOURCE_VGUI_FILEIMAGE_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/compiler_specific.h"
#include "base/include/base_types.h"

using FileHandle_t = void *;

the_interface FileImageStream {
 public:
  virtual void Read(void *pOut, int len) = 0;

  // Returns true if there were any Read errors.
  // Clears error status.
  virtual bool ErrorStatus() = 0;
};

// Use to read out of a memory buffer..
class FileImageStream_Memory : public FileImageStream {
 public:
  FileImageStream_Memory(void *pData, int dataLen);

  virtual void Read(void *pOut, int len);
  virtual bool ErrorStatus();

 private:
  u8 *m_pData;
  int m_DataLen;
  int m_CurPos;
  bool m_bError;
};

// Generic image representation..
class FileImage {
 public:
  FileImage() { Clear(); }
  ~FileImage() { Term(); }

  void Term() {
    delete[] m_pData;

    Clear();
  }

  // Clear the structure without deallocating.
  void Clear() {
    m_Width = m_Height = 0;
    m_pData = nullptr;
  }

  int m_Width, m_Height;
  u8 *m_pData;
};

// Functions to load/save FileImages.
bool Load32BitTGA(FileImageStream *fp, FileImage *pImage);
void Save32BitTGA(FileHandle_t fp, FileImage *pImage);

#endif  // SOURCE_VGUI_FILEIMAGE_H_
