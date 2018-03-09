// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "miles_base_types.h"

#include <atomic>
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

// TODO(d.rattman): Why Miles needs this for MSVC?
int __cdecl MSS_auto_cleanup() { return 0; }

static std::atomic_uint32_t global_miles_ref_count{0};

void IncrementRefMiles() {
  if (global_miles_ref_count == 0) {
    AIL_set_redist_directory("." MSS_REDIST_DIR_NAME);
    AIL_startup();
  }

  ++global_miles_ref_count;
}

void DecrementRefMiles() {
  Assert(global_miles_ref_count > 0);
  --global_miles_ref_count;

  if (global_miles_ref_count == 0) {
    MilesProviderCache::FreeAllProviders();
    AIL_shutdown();
  }
}

MilesProviderCache *MilesProviderCache::s_pHead = nullptr;

MilesProviderCache::MilesProviderCache(HPROVIDER provider) {
  m_hProvider = provider;

  // Add to the global list of CProviders.
  m_pNext = s_pHead;
  s_pHead = this;
}

MilesProviderCache::~MilesProviderCache() {
  RIB_free_provider_library(m_hProvider);

  // Remove from the global list.
  MilesProviderCache **prev = &s_pHead;
  for (auto *it = s_pHead; it; it = it->m_pNext) {
    if (it == this) {
      *prev = m_pNext;
      return;
    }

    prev = &it->m_pNext;
  }
}

MilesProviderCache *MilesProviderCache::FindProvider(HPROVIDER provider) {
  for (auto *it = s_pHead; it; it = it->m_pNext) {
    if (it->GetProviderHandle() == provider) {
      return it;
    }
  }
  return nullptr;
}

void MilesProviderCache::FreeAllProviders() {
  MilesProviderCache *next;
  for (auto *it = s_pHead; it; it = next) {
    next = it->m_pNext;
    delete it;
  }
}

HPROVIDER MilesProviderCache::GetProviderHandle() { return m_hProvider; }

ASISTRUCT::ASISTRUCT() {
  Clear();
  IncrementRefMiles();
}

ASISTRUCT::~ASISTRUCT() {
  Shutdown();
  DecrementRefMiles();
}

void ASISTRUCT::Clear() {
  m_pProvider = nullptr;
  ASI_stream_open = nullptr;
  ASI_stream_process = nullptr;
  ASI_stream_close = nullptr;
  ASI_stream_attribute = nullptr;
  ASI_stream_set_preference = nullptr;
  ASI_stream_seek = nullptr;
  OUTPUT_BITS = 0;
  OUTPUT_CHANNELS = 0;
  INPUT_BITS = 0;
  INPUT_CHANNELS = 0;
  POSITION = 0;
  m_stream = 0;
}

bool ASISTRUCT::Init(void *callback_object, const char *in_file_type,
                     const char *out_file_type, AILASIFETCHCB cb) {
  // Get the provider.
  HPROVIDER provider =
      RIB_find_files_provider("ASI codec", "Output file types", out_file_type,
                              "Input file types", in_file_type);
  if (!provider) {
    Error("Can't find provider 'ASI codec' for input %s, output %s",
          in_file_type, out_file_type);
    return false;
  }

  m_pProvider = MilesProviderCache::FindProvider(provider);
  if (!m_pProvider) {
    m_pProvider = new MilesProviderCache(provider);
  }

  if (!m_pProvider) {
    Error("Can't create provider 'ASI codec'");
    return false;
  }

  // Get the decoder.
  RIB_INTERFACE_ENTRY entries[] = {
      {RIB_FUNCTION, "ASI_stream_attribute", (U32)&ASI_stream_attribute,
       RIB_NONE},
      {RIB_FUNCTION, "ASI_stream_open", (U32)&ASI_stream_open, RIB_NONE},
      {RIB_FUNCTION, "ASI_stream_close", (U32)&ASI_stream_close, RIB_NONE},
      {RIB_FUNCTION, "ASI_stream_process", (U32)&ASI_stream_process, RIB_NONE},
      {RIB_FUNCTION, "ASI_stream_set_preference",
       (U32)&ASI_stream_set_preference, RIB_NONE},
      {RIB_ATTRIBUTE, "Output sample rate", (U32)&OUTPUT_RATE, RIB_NONE},
      {RIB_ATTRIBUTE, "Output sample width", (U32)&OUTPUT_BITS, RIB_NONE},
      {RIB_ATTRIBUTE, "Output channels", (U32)&OUTPUT_CHANNELS, RIB_NONE},
      {RIB_ATTRIBUTE, "Input sample rate", (U32)&INPUT_RATE, RIB_NONE},
      {RIB_ATTRIBUTE, "Input channels", (U32)&INPUT_CHANNELS, RIB_NONE},
      {RIB_ATTRIBUTE, "Input sample width", (U32)&INPUT_BITS, RIB_NONE},
      {RIB_ATTRIBUTE, "Minimum input block size", (U32)&INPUT_BLOCK_SIZE,
       RIB_NONE},
      {RIB_ATTRIBUTE, "Position", (U32)&POSITION, RIB_NONE},
  };

  RIBRESULT result_code =
      RIB_request(m_pProvider->GetProviderHandle(), "ASI stream", entries);
  if (result_code != RIB_NOERR) {
    Error("Can't find interface 'ASI stream' for provider 'ASI codec'");
    return false;
  }

  // This function doesn't exist for the voice DLLs, but it's not fatal in that
  // case.
  RIB_INTERFACE_ENTRY seek_entries[] = {
      {RIB_FUNCTION, "ASI_stream_seek", (U32)&ASI_stream_seek, RIB_NONE}};
  result_code =
      RIB_request(m_pProvider->GetProviderHandle(), "ASI stream", seek_entries);
  if (result_code != RIB_NOERR) {
    ASI_stream_seek = nullptr;
  }

  m_stream = ASI_stream_open((U32)callback_object, cb, 0);
  if (!m_stream) {
    Error("Can't open ASI stream");
    return false;
  }

  return true;
}

void ASISTRUCT::Shutdown() {
  if (m_pProvider) {
    if (m_stream && ASI_stream_close) {
      ASI_stream_close(m_stream);
      m_stream = 0;
    }

    // m_pProvider->Release();
    m_pProvider = nullptr;
  }

  Clear();
}

int ASISTRUCT::Process(void *pBuffer, unsigned int bufferSize) {
  return ASI_stream_process(m_stream, pBuffer, bufferSize);
}

bool ASISTRUCT::IsActive() const { return m_stream != 0; }

unsigned int ASISTRUCT::GetAttribute(HATTRIB attribute) {
  return ASI_stream_attribute(m_stream, attribute);
}

void ASISTRUCT::Seek(int position) {
  if (!ASI_stream_seek) Error("ASI_stream_seek called, but it doesn't exist.");

  ASI_stream_seek(m_stream, (S32)position);
}

void ASISTRUCT::SetAttribute(HATTRIB attribute, unsigned int value) {
  ASI_stream_set_preference(m_stream, attribute, &value);
}
