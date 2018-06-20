// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Interface for filesystem calls used by the saverestore system
// to manupulate the save directory.

#ifndef SAVERESTOREFILESYSTEM_H
#define SAVERESTOREFILESYSTEM_H

#include "base/include/base_types.h"
#include "filesystem.h"
#include "tier1/utlmap.h"
#include "tier1/utlsymbol.h"

the_interface ISaveRestoreFileSystem {
 public:
  virtual FileHandle_t Open(const ch *pFileName, const ch *pOptions,
                            const ch *pathID = nullptr) = 0;
  virtual void Close(FileHandle_t) = 0;
  virtual int Read(void *pOutput, int size, FileHandle_t file) = 0;
  virtual int Write(void const *pInput, int size, FileHandle_t file) = 0;
  virtual void Seek(FileHandle_t file, int pos, FileSystemSeek_t method) = 0;
  virtual unsigned int Tell(FileHandle_t file) = 0;
  virtual unsigned int Size(FileHandle_t file) = 0;
  virtual unsigned int Size(const ch *pFileName,
                            const ch *pPathID = nullptr) = 0;

  virtual bool FileExists(const ch *pFileName, const ch *pPathID = nullptr) = 0;
  virtual void RenameFile(ch const *pOldPath, ch const *pNewPath,
                          const ch *pathID = nullptr) = 0;
  virtual void RemoveFile(ch const *pRelativePath,
                          const ch *pathID = nullptr) = 0;

  virtual void AsyncFinishAllWrites() = 0;
  virtual void AsyncRelease(FSAsyncControl_t hControl) = 0;
  virtual FSAsyncStatus_t AsyncWrite(
      const ch *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory,
      bool bAppend, FSAsyncControl_t *pControl = nullptr) = 0;
  virtual FSAsyncStatus_t AsyncFinish(FSAsyncControl_t hControl,
                                      bool wait = false) = 0;
  virtual FSAsyncStatus_t AsyncAppend(const ch *pFileName, const void *pSrc,
                                      int nSrcBytes, bool bFreeMemory,
                                      FSAsyncControl_t *pControl = nullptr) = 0;
  virtual FSAsyncStatus_t AsyncAppendFile(
      const ch *pDestFileName, const ch *pSrcFileName,
      FSAsyncControl_t *pControl = nullptr) = 0;

  virtual void DirectoryCopy(const ch *pPath, const ch *pDestFileName) = 0;
  virtual bool DirectoryExtract(FileHandle_t pFile, int fileCount) = 0;
  virtual int DirectoryCount(const ch *pPath) = 0;
  virtual void DirectoryClear(const ch *pPath) = 0;

  virtual void AuditFiles() = 0;
  virtual bool LoadFileFromDisk(const ch *pFilename) = 0;
};

extern ISaveRestoreFileSystem *g_pSaveRestoreFileSystem;

#endif  // SAVERESTOREFILESYSTEM_H
