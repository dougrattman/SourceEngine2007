// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_AVI_IBIK_H_
#define SOURCE_AVI_IBIK_H_

#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"

// Parameters for creating a new BINK
struct BIKParams_t {
  BIKParams_t()
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

  char m_pFileName[256];
  char m_pPathID[256];

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

// Handle to an BINK
typedef unsigned short BIKHandle_t;
enum { BIKHANDLE_INVALID = (BIKHandle_t)~0 };

// Handle to an BINK material
typedef unsigned short BIKMaterial_t;
enum { BIKMATERIAL_INVALID = (BIKMaterial_t)~0 };

// Main AVI interface
#define BIK_INTERFACE_VERSION "VBik001"

src_interface IBik : public IAppSystem {
  // Create/destroy a BINK material (a materialsystem IMaterial)
  virtual BIKMaterial_t CreateMaterial(const ch *name, const ch *file_name,
                                       const ch *path_id) = 0;
  virtual void DestroyMaterial(BIKMaterial_t m) = 0;

  // Update the frame (if necessary)
  virtual bool Update(BIKMaterial_t m) = 0;

  // Gets the IMaterial associated with an BINK material
  virtual the_interface IMaterial *GetMaterial(BIKMaterial_t m) = 0;

  // Returns the max texture coordinate of the BINK
  virtual void GetTexCoordRange(BIKMaterial_t m, f32 *max_u, f32 *max_v) = 0;

  // Returns the frame size of the BINK (stored in a subrect of the material
  // itself)
  virtual void GetFrameSize(BIKMaterial_t m, i32 *width, i32 *height) = 0;
  // Returns the frame rate of the BINK
  virtual int GetFrameRate(BIKMaterial_t m) = 0;
  // Returns the total frame count of the BINK
  virtual int GetFrameCount(BIKMaterial_t m) = 0;
  // Sets the frame for an BINK material (use instead of SetTime)
  virtual void SetFrame(BIKMaterial_t m, f32 frame) = 0;

  // Sets the direct sound device that Bink will decode to
  virtual bool SetDirectSoundDevice(void *device) = 0;
};

#endif  // SOURCE_AVI_IBIK_H_
