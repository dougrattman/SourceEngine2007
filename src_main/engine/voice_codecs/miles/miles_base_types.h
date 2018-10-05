// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MILES_BASE_TYPES_H_
#define MILES_BASE_TYPES_H_

#include "build/include/build_config.h"

#ifdef OS_WIN
// Miles includes own windows.h, which is not protected.
// So do include our version before.
#include "base/include/windows/windows_light.h"
#endif

#include "deps/miles/include/mss.h"

class MilesProviderCache {
 public:
  MilesProviderCache(HPROVIDER miles_provider);

  static MilesProviderCache *FindProvider(HPROVIDER miles_provider);
  static void FreeAllProviders();

  HPROVIDER GetProviderHandle();

 private:
  ~MilesProviderCache();

 private:
  HPROVIDER m_hProvider;

  static MilesProviderCache *s_pHead;
  MilesProviderCache *m_pNext;
};

// This holds the handles and function pointers we want from a
// compressor/decompressor.
class ASISTRUCT {
 public:
  ASISTRUCT();
  ~ASISTRUCT();

  bool Init(void *pCallbackObject, const char *pInputFileType,
            const char *pOutputFileType, AILASIFETCHCB cb);
  void Shutdown();
  int Process(void *pBuffer, unsigned int bufferSize);
  bool IsActive() const;
  unsigned int GetAttribute(HATTRIB attribute);
  void SetAttribute(HATTRIB attribute, unsigned int value);
  void Seek(int position);

 public:
  HATTRIB OUTPUT_BITS;
  HATTRIB OUTPUT_CHANNELS;
  HATTRIB OUTPUT_RATE;

  HATTRIB INPUT_BITS;
  HATTRIB INPUT_CHANNELS;
  HATTRIB INPUT_RATE;
  HATTRIB INPUT_BLOCK_SIZE;
  HATTRIB POSITION;

 private:
  ASI_STREAM_OPEN ASI_stream_open;
  ASI_STREAM_PROCESS ASI_stream_process;
  ASI_STREAM_CLOSE ASI_stream_close;
  ASI_STREAM_SEEK ASI_stream_seek;
  ASI_STREAM_SET_PREFERENCE ASI_stream_set_preference;
  ASI_STREAM_ATTRIBUTE ASI_stream_attribute;

  HASISTREAM m_stream;
  MilesProviderCache *m_pProvider;

  
  void Clear();
};

extern void IncrementRefMiles();
extern void DecrementRefMiles();

#endif  // MILES_BASE_TYPES_H_
