// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER2_RIFF_H_
#define SOURCE_TIER2_RIFF_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "base/include/macros.h"

// This is a simple abstraction that the RIFF classes use to read from
// files/memory.
the_interface IFileReadBinary {
 public:
  virtual intptr_t open(const ch *pFileName) = 0;
  virtual int read(void *pOutput, int size, intptr_t file) = 0;
  virtual void close(intptr_t file) = 0;
  virtual void seek(intptr_t file, int pos) = 0;
  virtual u32 tell(intptr_t file) = 0;
  virtual u32 size(intptr_t file) = 0;
};

// Used to read/parse a RIFF format file.
class InFileRIFF {
 public:
  InFileRIFF(const ch *pFileName, IFileReadBinary &io);
  ~InFileRIFF();

  u32 RIFFName() const { return m_riffName; }
  u32 RIFFSize() const { return m_riffSize; }

  int ReadInt();
  int ReadData(void *pOutput, int dataSize);
  int PositionGet();
  void PositionSet(int position);
  bool IsValid() { return m_file != 0; }

 private:
  const InFileRIFF &operator=(const InFileRIFF &);

  IFileReadBinary &m_io;
  intptr_t m_file;
  u32 m_riffName;
  u32 m_riffSize;
};

// Used to iterate over an InFileRIFF.
class IterateRIFF {
 public:
  IterateRIFF(InFileRIFF &riff, int size);
  IterateRIFF(IterateRIFF &parent);

  bool ChunkAvailable();
  bool ChunkNext();

  u32 ChunkName();
  u32 ChunkSize();
  int ChunkRead(void *pOutput);
  int ChunkReadPartial(void *pOutput, int dataSize);
  int ChunkReadInt();
  int ChunkFilePosition() { return m_chunkPosition; }

 private:
  const IterateRIFF &operator=(const IterateRIFF &);

  void ChunkSetup();
  void ChunkClear();

  InFileRIFF &m_riff;
  int m_start;
  int m_size;

  u32 m_chunkName;
  int m_chunkSize;
  int m_chunkPosition;
};

the_interface IFileWriteBinary {
 public:
  virtual intptr_t create(const char *pFileName) = 0;
  virtual int write(void *pData, int size, intptr_t file) = 0;
  virtual void close(intptr_t file) = 0;
  virtual void seek(intptr_t file, int pos) = 0;
  virtual u32 tell(intptr_t file) = 0;
};

// Used to write a RIFF format file
class OutFileRIFF {
 public:
  OutFileRIFF(const char *pFileName, IFileWriteBinary &io);
  ~OutFileRIFF();

  bool WriteInt(int number);
  bool WriteData(void *pOutput, int dataSize);
  int PositionGet();
  void PositionSet(int position);
  bool IsValid() { return m_file != 0; }

  void HasLISETData(int position);

 private:
  const OutFileRIFF &operator=(const OutFileRIFF &);

  IFileWriteBinary &m_io;
  intptr_t m_file;
  u32 m_riffSize;
  u32 m_nNamePos;

  // hack to make liset work correctly
  bool m_bUseIncorrectLISETLength;
  int m_nLISETSize;
};

// Used to iterate over an InFileRIFF
class IterateOutputRIFF {
 public:
  IterateOutputRIFF(OutFileRIFF &riff);
  IterateOutputRIFF(IterateOutputRIFF &parent);

  void ChunkStart(u32 chunkname);
  void ChunkFinish();

  void ChunkWrite(u32 chunkname, void *pOutput, int size);
  void ChunkWriteInt(int number);
  void ChunkWriteData(void *pOutput, int size);

  int ChunkFilePosition() { return m_chunkPosition; }

  u32 ChunkGetPosition();
  void ChunkSetPosition(int position);

  void CopyChunkData(IterateRIFF &input);

  void SetLISETData(int position);

 private:
  const IterateOutputRIFF &operator=(const IterateOutputRIFF &);

  OutFileRIFF &m_riff;
  int m_start;
  int m_size;

  u32 m_chunkName;
  int m_chunkSize;
  int m_chunkPosition;
  int m_chunkStart;
};

#define RIFF_ID MAKEID<int32_t>('R', 'I', 'F', 'F')
#define RIFF_WAVE MAKEID('W', 'A', 'V', 'E')
#define WAVE_FMT MAKEID('f', 'm', 't', ' ')
#define WAVE_DATA MAKEID('d', 'a', 't', 'a')
#define WAVE_FACT MAKEID('f', 'a', 'c', 't')
#define WAVE_CUE MAKEID('c', 'u', 'e', ' ')
#define WAVE_SAMPLER MAKEID('s', 'm', 'p', 'l')
#define WAVE_VALVEDATA MAKEID('V', 'D', 'A', 'T')
#define WAVE_PADD MAKEID('P', 'A', 'D', 'D')
#define WAVE_LIST MAKEID('L', 'I', 'S', 'T')

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 0x0001
#endif
#ifndef WAVE_FORMAT_ADPCM
#define WAVE_FORMAT_ADPCM 0x0002
#endif
#define WAVE_FORMAT_XBOX_ADPCM 0x0069
#ifndef WAVE_FORMAT_XMA
#define WAVE_FORMAT_XMA 0x0165
#endif

#endif  // SOURCE_TIER2_RIFF_H_
