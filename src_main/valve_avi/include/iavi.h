// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VALVE_AVI_INCLUDE_IAVI_H_
#define SOURCE_VALVE_AVI_INCLUDE_IAVI_H_

#include "appframework/include/iapp_system.h"
#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"

// Parameters for creating a new AVI
struct AVIParams_t {
  AVIParams_t()
      : m_nFrameRate(0),
        m_nFrameScale(1),
        m_nWidth(0),
        m_nHeight(0),
        m_nSampleRate(0),
        m_nSampleBits(0),
        m_nNumChannels(0) {
    m_pFileName[0] = 0;
    m_pPathID[0] = 0;
  }

  ch m_pFileName[256];
  ch m_pPathID[256];

  // fps = m_nFrameRate / m_nFrameScale
  // for integer framerates, set framerate to the fps, and framescale to 1
  // for ntsc-style framerates like 29.97 (or 23.976 or 59.94),
  // set framerate to 30,000 (or 24,000 or 60,000) and framescale to 1001
  // yes, framescale is an odd naming choice, but it matching MS's AVI api
  int m_nFrameRate;
  int m_nFrameScale;

  int m_nWidth;
  int m_nHeight;

  // Sound/.wav info
  int m_nSampleRate;
  int m_nSampleBits;
  int m_nNumChannels;
};

// Handle to an AVI
using AVIHandle_t = u16;
constexpr inline AVIHandle_t AVIHANDLE_INVALID = (AVIHandle_t)~0;

// Handle to an AVI material
using AVIMaterial_t = u16;
constexpr inline AVIHandle_t AVIMATERIAL_INVALID = (AVIMaterial_t)~0;

// Main AVI interface
#define AVI_INTERFACE_VERSION "VAvi001"

src_interface IAvi : public IAppSystem {
  // Necessary to call this before any other AVI interface methods
  virtual void SetMainWindow(void *hwnd) = 0;

  // Start/stop recording an AVI
  virtual AVIHandle_t StartAVI(const AVIParams_t &params) = 0;
  virtual void FinishAVI(AVIHandle_t h) = 0;

  // Add frames to an AVI
  virtual void AppendMovieSound(AVIHandle_t h, i16 * buffer, usize size) = 0;
  virtual void AppendMovieFrame(AVIHandle_t h,
                                const struct BGR888_t *rgb_data) = 0;

  // Create/destroy an AVI material (a materialsystem IMaterial)
  virtual AVIMaterial_t CreateAVIMaterial(const ch *name, const ch *file_name,
                                          const ch *path_id) = 0;
  virtual void DestroyAVIMaterial(AVIMaterial_t m) = 0;

  // Sets the time for an AVI material
  virtual void SetTime(AVIMaterial_t m, float time) = 0;

  // Gets the IMaterial associated with an AVI material
  virtual the_interface IMaterial *GetMaterial(AVIMaterial_t m) = 0;

  // Returns the max texture coordinate of the AVI
  virtual void GetTexCoordRange(AVIMaterial_t m, f32 * max_u, f32 * max_v) = 0;

  // Returns the frame size of the AVI (stored in a subrect of the material
  // itself)
  virtual void GetFrameSize(AVIMaterial_t m, i32 * width, i32 * height) = 0;
  // Returns the frame rate of the AVI
  virtual int GetFrameRate(AVIMaterial_t m) = 0;
  // Returns the total frame count of the AVI
  virtual int GetFrameCount(AVIMaterial_t m) = 0;
  // Sets the frame for an AVI material (use instead of SetTime)
  virtual void SetFrame(AVIMaterial_t m, f32 frame) = 0;
};

#endif  // SOURCE_VALVE_AVI_INCLUDE_IAVI_H_
