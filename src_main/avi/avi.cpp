// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "avi/iavi.h"

#include "avi.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/materialsystemutil.h"
#include "pixelwriter.h"
#include "tier1/keyvalues.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier3/tier3.h"
#include "vtf/vtf.h"

#include "base/include/windows/scoped_device_context.h"
#include "base/include/windows/windows_light.h"

#include <vfw.h>

// Class used to write out AVI files
class CAviFile {
 public:
  CAviFile() { Reset(); }

  // Start recording an AVI
  void Init(const AVIParams_t &params, void *hWnd) {
    Reset();

    char avi_file_name[SOURCE_MAX_PATH];
    char full_avi_file_path[SOURCE_MAX_PATH];
    sprintf_s(avi_file_name, "%s", params.m_pFileName);
    Q_SetExtension(avi_file_name, ".avi", sizeof(avi_file_name));

    g_pFullFileSystem->RelativePathToFullPath(avi_file_name, params.m_pPathID,
                                              full_avi_file_path,
                                              sizeof(full_avi_file_path));
    if (g_pFullFileSystem->FileExists(full_avi_file_path, params.m_pPathID)) {
      g_pFullFileSystem->RemoveFile(full_avi_file_path, params.m_pPathID);
    }

    HRESULT hr = AVIFileOpen(&m_pAVIFile, full_avi_file_path,
                             OF_WRITE | OF_CREATE, nullptr);
    if (hr != AVIERR_OK) return;

    m_wFormat.cbSize = sizeof(m_wFormat);
    m_wFormat.wFormatTag = WAVE_FORMAT_PCM;
    m_wFormat.nChannels = params.m_nNumChannels;
    m_wFormat.nSamplesPerSec = params.m_nSampleRate;
    m_wFormat.nBlockAlign =
        params.m_nNumChannels * (params.m_nSampleBits == 8 ? 1 : 2);
    m_wFormat.nAvgBytesPerSec = m_wFormat.nBlockAlign * params.m_nSampleRate;
    m_wFormat.wBitsPerSample = params.m_nSampleBits;

    m_nFrameRate = params.m_nFrameRate;
    m_nFrameScale = params.m_nFrameScale;

    is_valid_ = true;

    height_ = params.m_nHeight;
    width_ = params.m_nWidth;

    CreateVideoStreams(params, hWnd);
    CreateAudioStream();
  }
  void Shutdown() {
    if (m_pAudioStream) {
      AVIStreamRelease(m_pAudioStream);
      m_pAudioStream = nullptr;
    }
    if (m_pVideoStream) {
      AVIStreamRelease(m_pVideoStream);
      m_pVideoStream = nullptr;
    }
    if (m_pCompressedStream) {
      AVIStreamRelease(m_pCompressedStream);
      m_pCompressedStream = nullptr;
    }

    if (m_pAVIFile) {
      AVIFileRelease(m_pAVIFile);
      m_pAVIFile = nullptr;
    }

    if (m_DIBSection != nullptr) {
      DeleteObject(m_DIBSection);
    }

    if (m_memdc != nullptr) {
      // Release the compatible DC
      DeleteDC(m_memdc);
    }

    Reset();
  }
  void AppendMovieSound(short *buf, size_t bufsize);
  void AppendMovieFrame(const BGR888_t *pRGBData);

 private:
  // Reset the avi file
  void Reset() {
    memset(&m_wFormat, 0, sizeof(m_wFormat));
    memset(&m_bi, 0, sizeof(m_bi));

    is_valid_ = false;
    width_ = 0;
    height_ = 0;
    m_pAVIFile = nullptr;
    m_nFrameRate = 0;
    m_nFrameScale = 1;
    m_pAudioStream = nullptr;
    m_pVideoStream = nullptr;
    m_pCompressedStream = nullptr;
    m_nFrame = 0;
    m_nSample = 0;
    m_memdc = (HDC)0;
    m_DIBSection = (HBITMAP)0;

    m_bih = &m_bi.bmiHeader;
    m_bih->biSize = sizeof(*m_bih);
    // m_bih->biWidth		= xxx
    // m_bih->biHeight		= xxx
    m_bih->biPlanes = 1;
    m_bih->biBitCount = 24;
    m_bih->biCompression = BI_RGB;
    // m_bih->biSizeImage	= ( ( m_bih->biWidth * m_bih->biBitCount/8 + 3
    // )& 0xFFFFFFFC ) * m_bih->biHeight;
    m_bih->biXPelsPerMeter = 10000;
    m_bih->biYPelsPerMeter = 10000;
    m_bih->biClrUsed = 0;
    m_bih->biClrImportant = 0;
  }
  void CreateVideoStreams(const AVIParams_t &params, void *hWnd);
  void CreateAudioStream();

  bool is_valid_;
  int width_, height_;
  IAVIFile *m_pAVIFile;
  WAVEFORMATEX m_wFormat;
  int m_nFrameRate;
  int m_nFrameScale;
  IAVIStream *m_pAudioStream;
  IAVIStream *m_pVideoStream;
  IAVIStream *m_pCompressedStream;
  int m_nFrame;
  int m_nSample;
  HDC m_memdc;
  HBITMAP m_DIBSection;
  BITMAPINFO m_bi;
  BITMAPINFOHEADER *m_bih;
};

template <size_t error_size>
static size_t FormatAviMessage(HRESULT code, char (&error_buffer)[error_size]) {
  const char *msg = "unknown avi result code";
  switch (code) {
    case AVIERR_OK:
      msg = "Success";
      break;
    case AVIERR_BADFORMAT:
      msg = "AVIERR_BADFORMAT: corrupt file or unrecognized format";
      break;
    case AVIERR_MEMORY:
      msg = "AVIERR_MEMORY: insufficient memory";
      break;
    case AVIERR_FILEREAD:
      msg = "AVIERR_FILEREAD: disk error while reading file";
      break;
    case AVIERR_FILEOPEN:
      msg = "AVIERR_FILEOPEN: disk error while opening file";
      break;
    case REGDB_E_CLASSNOTREG:
      msg = "REGDB_E_CLASSNOTREG: file type not recognised";
      break;
    case AVIERR_READONLY:
      msg = "AVIERR_READONLY: file is read-only";
      break;
    case AVIERR_NOCOMPRESSOR:
      msg = "AVIERR_NOCOMPRESSOR: a suitable compressor could not be found";
      break;
    case AVIERR_UNSUPPORTED:
      msg =
          "AVIERR_UNSUPPORTED: compression is not supported for this type of "
          "data";
      break;
    case AVIERR_INTERNAL:
      msg = "AVIERR_INTERNAL: internal error";
      break;
    case AVIERR_BADFLAGS:
      msg = "AVIERR_BADFLAGS";
      break;
    case AVIERR_BADPARAM:
      msg = "AVIERR_BADPARAM";
      break;
    case AVIERR_BADSIZE:
      msg = "AVIERR_BADSIZE";
      break;
    case AVIERR_BADHANDLE:
      msg = "AVIERR_BADHANDLE";
      break;
    case AVIERR_FILEWRITE:
      msg = "AVIERR_FILEWRITE: disk error while writing file";
      break;
    case AVIERR_COMPRESSOR:
      msg = "AVIERR_COMPRESSOR";
      break;
    case AVIERR_NODATA:
      msg = "AVIERR_READONLY";
      break;
    case AVIERR_BUFFERTOOSMALL:
      msg = "AVIERR_BUFFERTOOSMALL";
      break;
    case AVIERR_CANTCOMPRESS:
      msg = "AVIERR_CANTCOMPRESS";
      break;
    case AVIERR_USERABORT:
      msg = "AVIERR_USERABORT";
      break;
    case AVIERR_ERROR:
      msg = "AVIERR_ERROR";
      break;
  }

  size_t mlen = strlen(msg);
  if (error_buffer == 0 || error_size == 0) return mlen;

  size_t n = mlen;
  if (n > error_size - 1) n = error_size - 1;

  strcpy_s(error_buffer, msg);
  return mlen;
}

static void ReportError(HRESULT hr) {
  char buf[512];
  FormatAviMessage(hr, buf);
  Warning("%s\n", buf);
}

void CAviFile::CreateVideoStreams(const AVIParams_t &params, void *hWnd) {
  AVISTREAMINFO stream_info = {0};
  stream_info.fccType = streamtypeVIDEO;
  stream_info.fccHandler = 0;
  stream_info.dwScale = params.m_nFrameScale;
  stream_info.dwRate = params.m_nFrameRate;
  stream_info.dwSuggestedBufferSize = params.m_nWidth * params.m_nHeight * 3;

  SetRect(&stream_info.rcFrame, 0, 0, params.m_nWidth, params.m_nHeight);

  HRESULT hr = AVIFileCreateStream(m_pAVIFile, &m_pVideoStream, &stream_info);
  if (hr != AVIERR_OK) {
    is_valid_ = false;
    ReportError(hr);
    return;
  }

  AVICOMPRESSOPTIONS compress_options = {0};
  // Choose DIVX compressor for now
  Warning("TODO(d.rattman):  DIVX only for now\n");
  compress_options.fccHandler = mmioFOURCC('d', 'i', 'b', ' ');

  AVICOMPRESSOPTIONS *compress_options_array[1]{&compress_options};

  // TODO(d.rattman): This won't work so well in full screen!!!
  BOOL res = (BOOL)AVISaveOptions((HWND)hWnd, 0, 1, &m_pVideoStream,
                                  compress_options_array);
  if (!res) {
    is_valid_ = false;
    return;
  }

  hr = AVIMakeCompressedStream(&m_pCompressedStream, m_pVideoStream,
                               &compress_options, nullptr);
  if (hr != AVIERR_OK) {
    is_valid_ = false;
    ReportError(hr);
    return;
  }

  // Create a compatible DC
  source::windows::ScopedDeviceContext dc{GetDesktopWindow()};
  m_memdc = dc.CreateCompatibleDC();

  // Set up a DIBSection for the screen
  m_bih->biWidth = params.m_nWidth;
  m_bih->biHeight = params.m_nHeight;
  m_bih->biSizeImage =
      ((m_bih->biWidth * m_bih->biBitCount / 8 + 3) & 0xFFFFFFFC) *
      m_bih->biHeight;

  // Create the DIBSection
  void *bits;
  m_DIBSection = CreateDIBSection(m_memdc, (BITMAPINFO *)m_bih, DIB_RGB_COLORS,
                                  &bits, nullptr, 0);

  // Get at the DIBSection object
  DIBSECTION dibs;
  GetObject(m_DIBSection, sizeof(dibs), &dibs);

  // Set the stream format
  hr = AVIStreamSetFormat(
      m_pCompressedStream, 0, &dibs.dsBmih,
      dibs.dsBmih.biSize + dibs.dsBmih.biClrUsed * sizeof(RGBQUAD));

  if (hr != AVIERR_OK) {
    is_valid_ = false;
    ReportError(hr);
    return;
  }
}

void CAviFile::CreateAudioStream() {
  AVISTREAMINFO audio_stream = {0};
  audio_stream.fccType = streamtypeAUDIO;
  audio_stream.dwScale = m_wFormat.nBlockAlign;
  audio_stream.dwRate = m_wFormat.nSamplesPerSec * m_wFormat.nBlockAlign;
  audio_stream.dwSampleSize = m_wFormat.nBlockAlign;
  audio_stream.dwQuality = (DWORD)-1;

  HRESULT hr = AVIFileCreateStream(m_pAVIFile, &m_pAudioStream, &audio_stream);
  if (hr == AVIERR_OK) {
    hr = AVIStreamSetFormat(m_pAudioStream, 0, &m_wFormat, sizeof(m_wFormat));
  }

  if (hr != AVIERR_OK) {
    is_valid_ = false;
    ReportError(hr);
  }
}

void CAviFile::AppendMovieSound(short *buf, size_t bufsize) {
  if (!is_valid_) return;

  long numsamps =
      bufsize / sizeof(short);  // numbytes*8 / au->wfx.wBitsPerSample;
  // now we can write the data
  HRESULT hr = AVIStreamWrite(m_pAudioStream, m_nSample, numsamps, buf, bufsize,
                              0, nullptr, nullptr);
  if (hr == AVIERR_OK) {
    m_nSample += numsamps;
  } else {
    is_valid_ = false;
    ReportError(hr);
  }
}

//-----------------------------------------------------------------------------
// Adds a frame of the movie to the AVI
//-----------------------------------------------------------------------------
void CAviFile::AppendMovieFrame(const BGR888_t *pRGBData) {
  if (!is_valid_) return;

  HGDIOBJ hOldObject = SelectObject(m_memdc, m_DIBSection);

  // Update the DIBSection bits
  // TODO(d.rattman): Have to invert this vertically since passing in negative
  // biHeights in the m_bih field doesn't make the system know it's a top-down
  // AVI
  int scanlines = 0;
  for (int i = 0; i < height_; ++i) {
    scanlines += SetDIBits(m_memdc, m_DIBSection, height_ - i - 1, 1, pRGBData,
                           (CONST BITMAPINFO *)m_bih, DIB_RGB_COLORS);
    pRGBData += width_;
  }

  DIBSECTION dibs;
  int objectSize = GetObject(m_DIBSection, sizeof(dibs), &dibs);
  if (scanlines != height_ || objectSize != sizeof(DIBSECTION)) {
    SelectObject(m_memdc, hOldObject);
    is_valid_ = false;
    return;
  }

  // Now we can add the frame
  HRESULT hr =
      AVIStreamWrite(m_pCompressedStream, m_nFrame, 1, dibs.dsBm.bmBits,
                     dibs.dsBmih.biSizeImage, AVIIF_KEYFRAME, nullptr, nullptr);

  SelectObject(m_memdc, hOldObject);

  if (hr != AVIERR_OK) {
    is_valid_ = false;
    ReportError(hr);
    return;
  }

  ++m_nFrame;
}

// Class used to associated AVI files with IMaterials
class CAVIMaterial : public ITextureRegenerator {
 public:
  CAVIMaterial();

  // Initializes, shuts down the material
  bool Init(const char *pMaterialName, const char *pFileName,
            const char *pPathID);
  void Shutdown();

  // Inherited from ITextureRegenerator
  virtual void RegenerateTextureBits(ITexture *pTexture,
                                     IVTFTexture *pVTFTexture, Rect_t *pRect);
  virtual void Release();

  // Returns the material
  IMaterial *GetMaterial();

  // Returns the texcoord range
  void GetTexCoordRange(float *pMaxU, float *pMaxV);

  // Returns the frame size of the AVI (stored in a subrect of the material
  // itself)
  void GetFrameSize(int *pWidth, int *pHeight);

  // Sets the current time
  void SetTime(float flTime);

  // Returns the frame rate/count of the AVI
  int GetFrameRate();
  int GetFrameCount();

  // Sets the frame for an AVI material (use instead of SetTime)
  void SetFrame(float flFrame);

 private:
  // Initializes, shuts down the procedural texture
  void CreateProceduralTexture(const char *pTextureName);
  void DestroyProceduralTexture();

  // Initializes, shuts down the procedural material
  void CreateProceduralMaterial(const char *pMaterialName);
  void DestroyProceduralMaterial();

  // Initializes, shuts down the video stream
  void CreateVideoStream();
  void DestroyVideoStream();

  CMaterialReference m_Material;
  CTextureReference m_Texture;
  IAVIFile *m_pAVIFile;
  IAVIStream *m_pAVIStream;
  IGetFrame *m_pGetFrame;
  int m_nAVIWidth;
  int m_nAVIHeight;
  int m_nFrameRate;
  int m_nFrameCount;
  int m_nCurrentSample;
  HDC m_memdc;
  HBITMAP m_DIBSection;
  BITMAPINFO m_bi;
  BITMAPINFOHEADER *m_bih;
};

CAVIMaterial::CAVIMaterial() {
  Q_memset(&m_bi, 0, sizeof(m_bi));
  m_memdc = (HDC)0;
  m_DIBSection = (HBITMAP)0;
  m_pAVIStream = nullptr;
  m_pAVIFile = nullptr;
  m_pGetFrame = nullptr;
}

// Initializes the material
bool CAVIMaterial::Init(const char *pMaterialName, const char *pFileName,
                        const char *pPathID) {
  // Determine the full path name of the AVI
  char pAVIFileName[512];
  char pFullAVIFileName[512];
  Q_snprintf(pAVIFileName, sizeof(pAVIFileName), "%s", pFileName);
  Q_DefaultExtension(pAVIFileName, ".avi", sizeof(pAVIFileName));
  g_pFullFileSystem->RelativePathToFullPath(
      pAVIFileName, pPathID, pFullAVIFileName, sizeof(pFullAVIFileName));

  HRESULT hr = AVIFileOpen(&m_pAVIFile, pFullAVIFileName, OF_READ, nullptr);
  if (hr != AVIERR_OK) {
    Warning("AVI '%s' not found\n", pFullAVIFileName);
    m_nAVIWidth = 64;
    m_nAVIHeight = 64;
    m_nFrameRate = 1;
    m_nFrameCount = 1;
    m_Material.Init("debug/debugempty", TEXTURE_GROUP_OTHER);
    return false;
  }

  // Get AVI size
  AVIFILEINFO info;
  AVIFileInfo(m_pAVIFile, &info, sizeof(info));
  m_nAVIWidth = info.dwWidth;
  m_nAVIHeight = info.dwHeight;
  m_nFrameRate = (int)((float)info.dwRate / (float)info.dwScale + 0.5f);
  m_nFrameCount = info.dwLength;
  CreateProceduralTexture(pMaterialName);
  CreateProceduralMaterial(pMaterialName);
  CreateVideoStream();

  m_Texture->Download();

  return true;
}

void CAVIMaterial::Shutdown() {
  DestroyVideoStream();
  DestroyProceduralMaterial();
  DestroyProceduralTexture();
  if (m_pAVIFile) {
    AVIFileRelease(m_pAVIFile);
    m_pAVIFile = nullptr;
  }
}

// Returns the material
IMaterial *CAVIMaterial::GetMaterial() { return m_Material; }

// Returns the texcoord range
void CAVIMaterial::GetTexCoordRange(float *pMaxU, float *pMaxV) {
  if (!m_Texture) {
    *pMaxU = *pMaxV = 1.0f;
    return;
  }

  int nTextureWidth = m_Texture->GetActualWidth();
  int nTextureHeight = m_Texture->GetActualHeight();
  *pMaxU = (float)m_nAVIWidth / (float)nTextureWidth;
  *pMaxV = (float)m_nAVIHeight / (float)nTextureHeight;
}

// Returns the frame size of the AVI (stored in a subrect of the material
// itself)
void CAVIMaterial::GetFrameSize(int *pWidth, int *pHeight) {
  *pWidth = m_nAVIWidth;
  *pHeight = m_nAVIHeight;
}

// Computes a power of two at least as big as the passed-in number
static inline int ComputeGreaterPowerOfTwo(int n) {
  int i = 1;
  while (i < n) {
    i <<= 1;
  }
  return i;
}

// Initializes, shuts down the procedural texture
void CAVIMaterial::CreateProceduralTexture(const char *pTextureName) {
  // Choose power-of-two textures which are at least as big as the AVI
  int nWidth = ComputeGreaterPowerOfTwo(m_nAVIWidth);
  int nHeight = ComputeGreaterPowerOfTwo(m_nAVIHeight);
  m_Texture.InitProceduralTexture(
      pTextureName, "avi", nWidth, nHeight, IMAGE_FORMAT_RGBA8888,
      TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP |
          TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY);
  m_Texture->SetTextureRegenerator(this);
}

void CAVIMaterial::DestroyProceduralTexture() {
  if (m_Texture) {
    m_Texture->SetTextureRegenerator(nullptr);
    m_Texture.Shutdown();
  }
}

// Initializes, shuts down the procedural material
void CAVIMaterial::CreateProceduralMaterial(const char *pMaterialName) {
  // TODO(d.rattman): gak, this is backwards.  Why doesn't the material just see
  // that it has a funky basetexture?
  char vmtfilename[512];
  strcpy_s(vmtfilename, pMaterialName);
  Q_SetExtension(vmtfilename, ".vmt", sizeof(vmtfilename));

  KeyValues *pVMTKeyValues = new KeyValues("UnlitGeneric");
  if (!pVMTKeyValues->LoadFromFile(g_pFullFileSystem, vmtfilename, "GAME")) {
    pVMTKeyValues->SetString("$basetexture", m_Texture->GetName());
    pVMTKeyValues->SetInt("$nofog", 1);
    pVMTKeyValues->SetInt("$spriteorientation", 3);
    pVMTKeyValues->SetInt("$translucent", 1);
  }

  m_Material.Init(pMaterialName, pVMTKeyValues);
  m_Material->Refresh();
}

void CAVIMaterial::DestroyProceduralMaterial() { m_Material.Shutdown(); }

// Sets the current time
void CAVIMaterial::SetTime(float flTime) {
  if (m_pAVIStream) {
    // Round to the nearest frame
    // TODO(d.rattman): Strangely, AVIStreamTimeToSample gets off by several
    // frames if you're a ways down the stream
    //		int nCurrentSample = AVIStreamTimeToSample( m_pAVIStream, (
    // flTime
    //+  0.5f / m_nFrameRate )* 1000.0f );
    int nCurrentSample = (int)(flTime * m_nFrameRate + 0.5f);
    if (m_nCurrentSample != nCurrentSample) {
      m_nCurrentSample = nCurrentSample;
      m_Texture->Download();
    }
  }
}

// Returns the frame rate of the AVI
int CAVIMaterial::GetFrameRate() { return m_nFrameRate; }

int CAVIMaterial::GetFrameCount() { return m_nFrameCount; }

// Sets the frame for an AVI material (use instead of SetTime)
void CAVIMaterial::SetFrame(float flFrame) {
  if (m_pAVIStream) {
    int nCurrentSample = (int)(flFrame + 0.5f);
    if (m_nCurrentSample != nCurrentSample) {
      m_nCurrentSample = nCurrentSample;
      m_Texture->Download();
    }
  }
}

// Initializes, shuts down the video stream
void CAVIMaterial::CreateVideoStream() {
  HRESULT hr = AVIFileGetStream(m_pAVIFile, &m_pAVIStream, streamtypeVIDEO, 0);
  if (hr != AVIERR_OK) {
    ReportError(hr);
    return;
  }

  m_nCurrentSample = AVIStreamStart(m_pAVIStream);

  // Create a compatible DC
  source::windows::ScopedDeviceContext dc{GetDesktopWindow()};
  m_memdc = dc.CreateCompatibleDC();

  // Set up a DIBSection for the screen
  m_bih = &m_bi.bmiHeader;
  m_bih->biSize = sizeof(*m_bih);
  m_bih->biWidth = m_nAVIWidth;
  m_bih->biHeight = m_nAVIHeight;
  m_bih->biPlanes = 1;
  m_bih->biBitCount = 32;
  m_bih->biCompression = BI_RGB;
  m_bih->biSizeImage =
      ((m_bih->biWidth * m_bih->biBitCount / 8 + 3) & 0xFFFFFFFC) *
      m_bih->biHeight;
  m_bih->biXPelsPerMeter = 10000;
  m_bih->biYPelsPerMeter = 10000;
  m_bih->biClrUsed = 0;
  m_bih->biClrImportant = 0;

  // Create the DIBSection
  void *bits;
  m_DIBSection = CreateDIBSection(m_memdc, (BITMAPINFO *)m_bih, DIB_RGB_COLORS,
                                  &bits, nullptr, 0);

  // Get at the DIBSection object
  DIBSECTION dibs;
  GetObject(m_DIBSection, sizeof(dibs), &dibs);

  m_pGetFrame = AVIStreamGetFrameOpen(m_pAVIStream, &dibs.dsBmih);
}

void CAVIMaterial::DestroyVideoStream() {
  if (m_pGetFrame) {
    AVIStreamGetFrameClose(m_pGetFrame);
    m_pGetFrame = nullptr;
  }

  if (m_DIBSection != nullptr) {
    DeleteObject(m_DIBSection);
    m_DIBSection = nullptr;
  }

  if (m_memdc != nullptr) {
    // Release the compatible DC
    DeleteDC(m_memdc);
    m_memdc = nullptr;
  }

  if (m_pAVIStream) {
    AVIStreamRelease(m_pAVIStream);
    m_pAVIStream = nullptr;
  }
}

// Inherited from ITextureRegenerator
void CAVIMaterial::RegenerateTextureBits(ITexture *pTexture,
                                         IVTFTexture *pVTFTexture,
                                         Rect_t *pRect) {
  CPixelWriter pixelWriter;
  LPBITMAPINFOHEADER lpbih;
  u8 *pData;
  int i, y, nIncY;

  // Error condition
  if (!m_pAVIStream || !m_pGetFrame || (pVTFTexture->FrameCount() > 1) ||
      (pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) ||
      (pVTFTexture->Depth() > 1)) {
    int nBytes = pVTFTexture->ComputeTotalSize();
    memset(pVTFTexture->ImageData(), 0xFF, nBytes);
    return;
  }

  lpbih = (LPBITMAPINFOHEADER)AVIStreamGetFrame(m_pGetFrame, m_nCurrentSample);
  if (!lpbih) {
    int nBytes = pVTFTexture->ComputeTotalSize();
    memset(pVTFTexture->ImageData(), 0xFF, nBytes);
    return;
  }

  // Set up the pixel writer to write into the VTF texture
  pixelWriter.SetPixelMemory(pVTFTexture->Format(), pVTFTexture->ImageData(),
                             pVTFTexture->RowSizeInBytes(0));

  int nWidth = pVTFTexture->Width();
  int nHeight = pVTFTexture->Height();
  int nBihHeight = abs(lpbih->biHeight);
  if (lpbih->biWidth > nWidth || nBihHeight > nHeight) {
    int nBytes = pVTFTexture->ComputeTotalSize();
    memset(pVTFTexture->ImageData(), 0xFF, nBytes);
    return;
  }

  pData = (u8 *)lpbih + lpbih->biSize;
  if (lpbih->biBitCount == 8) {
    // This is the palette
    pData += 256 * sizeof(RGBQUAD);
  }

  if (((lpbih->biBitCount == 16) || (lpbih->biBitCount == 32)) &&
      (lpbih->biCompression == BI_BITFIELDS)) {
    pData += 3 * sizeof(DWORD);

    // MASKS NOT IMPLEMENTED YET
    Assert(0);
  }

  int nStride = (lpbih->biWidth * lpbih->biBitCount / 8 + 3) & 0xFFFFFFFC;
  if (lpbih->biHeight > 0) {
    y = nBihHeight - 1;
    nIncY = -1;
  } else {
    y = 0;
    nIncY = 1;
  }

  if (lpbih->biBitCount == 24) {
    for (i = 0; i < nBihHeight; ++i, pData += nStride, y += nIncY) {
      pixelWriter.Seek(0, y);
      BGR888_t *pAVIPixel = (BGR888_t *)pData;
      for (int x = 0; x < lpbih->biWidth; ++x, ++pAVIPixel) {
        pixelWriter.WritePixel(pAVIPixel->r, pAVIPixel->g, pAVIPixel->b, 255);
      }
    }
  } else if (lpbih->biBitCount == 32) {
    for (i = 0; i < nBihHeight; ++i, pData += nStride, y += nIncY) {
      pixelWriter.Seek(0, y);
      BGRA8888_t *pAVIPixel = (BGRA8888_t *)pData;
      for (int x = 0; x < lpbih->biWidth; ++x, ++pAVIPixel) {
        pixelWriter.WritePixel(pAVIPixel->r, pAVIPixel->g, pAVIPixel->b,
                               pAVIPixel->a);
      }
    }
  }
  return;
}

void CAVIMaterial::Release() {}

// Implementation of IAvi
class CAvi : public CTier3AppSystem<IAvi> {
  typedef CTier3AppSystem<IAvi> BaseClass;

 public:
  CAvi() : m_hWnd{nullptr} {}

  // Inherited from IAppSystem

  // Connect/disconnect
  bool Connect(CreateInterfaceFn factory) override {
    if (!BaseClass::Connect(factory)) return false;
    if (!(g_pFullFileSystem && materials)) {
      Msg("Avi failed to connect to a required system\n");
    }
    return (g_pFullFileSystem && materials);
  }
  // Query Interface
  void *QueryInterface(const char *pInterfaceName) override {
    if (!strncmp(pInterfaceName, AVI_INTERFACE_VERSION,
                 strlen(AVI_INTERFACE_VERSION) + 1))
      return implicit_cast<IAvi *>(this);

    return nullptr;
  }
  InitReturnVal_t Init() override {
    InitReturnVal_t nRetVal = BaseClass::Init();
    if (nRetVal != INIT_OK) return nRetVal;

    AVIFileInit();
    return INIT_OK;
  }
  void Shutdown() override {
    AVIFileExit();
    BaseClass::Shutdown();
  }

  // Inherited from IAvi

  // Sets the main window
  void SetMainWindow(void *hWnd) override { m_hWnd = (HWND)hWnd; }

  // Start, finish recording an AVI
  AVIHandle_t StartAVI(const AVIParams_t &params) override {
    AVIHandle_t h = m_AVIFiles.AddToTail();
    m_AVIFiles[h].Init(params, m_hWnd);
    return h;
  }

  void FinishAVI(AVIHandle_t h) override {
    if (h != AVIHANDLE_INVALID) {
      m_AVIFiles[h].Shutdown();
      m_AVIFiles.Remove(h);
    }
  }
  // Add sound buffer
  void AppendMovieSound(AVIHandle_t h, short *buf, size_t bufsize) override {
    if (h != AVIHANDLE_INVALID) {
      m_AVIFiles[h].AppendMovieSound(buf, bufsize);
    }
  }
  void AppendMovieFrame(AVIHandle_t h, const BGR888_t *pRGBData) override {
    if (h != AVIHANDLE_INVALID) {
      m_AVIFiles[h].AppendMovieFrame(pRGBData);
    }
  }
  // Create/destroy an AVI material
  AVIMaterial_t CreateAVIMaterial(const char *pMaterialName,
                                  const char *pFileName,
                                  const char *pPathID) override {
    AVIMaterial_t h = m_AVIMaterials.AddToTail();
    m_AVIMaterials[h] = new CAVIMaterial;
    m_AVIMaterials[h]->Init(pMaterialName, pFileName, pPathID);
    return h;
  }
  void DestroyAVIMaterial(AVIMaterial_t h) override {
    if (h != AVIMATERIAL_INVALID) {
      m_AVIMaterials[h]->Shutdown();
      delete m_AVIMaterials[h];
      m_AVIMaterials.Remove(h);
    }
  }
  // Sets the time for an AVI material
  void SetTime(AVIMaterial_t h, float flTime) override {
    if (h != AVIMATERIAL_INVALID) {
      m_AVIMaterials[h]->SetTime(flTime);
    }
  }
  // Gets the IMaterial associated with an AVI material
  IMaterial *GetMaterial(AVIMaterial_t h) override {
    if (h != AVIMATERIAL_INVALID) return m_AVIMaterials[h]->GetMaterial();
    return nullptr;
  }

  // Returns the max texture coordinate of the AVI
  void GetTexCoordRange(AVIMaterial_t h, float *pMaxU, float *pMaxV) override {
    if (h != AVIMATERIAL_INVALID) {
      m_AVIMaterials[h]->GetTexCoordRange(pMaxU, pMaxV);
    } else {
      *pMaxU = *pMaxV = 1.0f;
    }
  }

  // Returns the frame size of the AVI (is a subrect of the material itself)
  void GetFrameSize(AVIMaterial_t h, int *pWidth, int *pHeight) override {
    if (h != AVIMATERIAL_INVALID) {
      m_AVIMaterials[h]->GetFrameSize(pWidth, pHeight);
    } else {
      *pWidth = *pHeight = 1;
    }
  }

  // Returns the frame rate of the AVI
  int GetFrameRate(AVIMaterial_t h) override {
    if (h != AVIMATERIAL_INVALID) return m_AVIMaterials[h]->GetFrameRate();
    return 1;
  }

  // Sets the frame for an AVI material (use instead of SetTime)
  void SetFrame(AVIMaterial_t hMaterial, float flFrame) override {
    if (hMaterial != AVIMATERIAL_INVALID) {
      m_AVIMaterials[hMaterial]->SetFrame(flFrame);
    }
  }

  int GetFrameCount(AVIMaterial_t hMaterial) override {
    if (hMaterial != AVIMATERIAL_INVALID)
      return m_AVIMaterials[hMaterial]->GetFrameCount();
    return 1;
  }

 private:
  HWND m_hWnd;
  CUtlLinkedList<CAviFile, AVIHandle_t> m_AVIFiles;

  // NOTE: Have to use pointers here since AVIMaterials inherit from
  // ITextureRegenerator The realloc screws up the pointers held to
  // ITextureRegenerators in the material system.
  CUtlLinkedList<CAVIMaterial *, AVIMaterial_t> m_AVIMaterials;
};

// Singleton
static CAvi g_AVI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CAvi, IAvi, AVI_INTERFACE_VERSION, g_AVI);
