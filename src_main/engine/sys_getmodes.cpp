// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include "ivideomode.h"

#include "IHammer.h"
#include "avi/iavi.h"
#include "bitmap/tgawriter.h"
#include "cl_main.h"
#include "cmd.h"
#include "common.h"
#include "deps/libjpeg/jpeglib.h"
#include "engine_launcher_api.h"
#include "filesystem_engine.h"
#include "gl_matsysiface.h"
#include "gl_model_private.h"
#include "gl_shader.h"
#include "host.h"
#include "iengine.h"
#include "igame.h"
#include "iregistry.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/itexture.h"
#include "materialsystem/materialsystem_config.h"
#include "modelloader.h"
#include "modes.h"
#include "sys_dll.h"
#include "sysexternal.h"
#include "tier0/include/icommandline.h"
#include "tier2/renderutils.h"
#include "tier2/tier2.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "vmodes.h"
#include "vtf/vtf.h"
#include "xbox/xboxstubs.h"

#include "tier0/include/memdbgon.h"

void CL_GetBackgroundLevelName(char *pszBackgroundName, int bufSize,
                               bool bMapName);

// HDRFIXME: move this somewhere else.
static void PFMWrite(float *pFloatImage, const char *pFilename, int width,
                     int height) {
  FileHandle_t fp = g_pFileSystem->Open(pFilename, "wb");
  g_pFileSystem->FPrintf(fp, "PF\n%d %d\n-1.000000\n", width, height);

  for (int i = height - 1; i >= 0; i--) {
    float *pRow = &pFloatImage[3 * width * i];
    g_pFileSystem->Write(pRow, width * sizeof(float) * 3, fp);
  }

  g_pFileSystem->Close(fp);
}

// Purpose: Expanded data destination object for CUtlBuffer output
struct JPEGDestinationManager_t {
  struct jpeg_destination_mgr pub;  // public fields

  CUtlBuffer *pBuffer;  // target/final buffer
  byte *buffer;         // start of temp buffer
};

// choose an efficiently bufferaable size
#define OUTPUT_BUF_SIZE 4096

// Purpose:  Initialize destination --- called by jpeg_start_compress
//  before any data is actually written.

METHODDEF(void) init_destination(j_compress_ptr cinfo) {
  JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *)cinfo->dest;

  // Allocate the output buffer --- it will be released when done with image
  dest->buffer = (uint8_t *)(*cinfo->mem->alloc_small)(
      (j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(byte));

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

// Purpose: Empty the output buffer --- called whenever buffer fills up.
// Input  : boolean -

METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo) {
  JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *)cinfo->dest;

  CUtlBuffer *buf = dest->pBuffer;

  // Add some data
  buf->Put(dest->buffer, OUTPUT_BUF_SIZE);

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

  return TRUE;
}

// Purpose: Terminate destination --- called by jpeg_finish_compress
// after all data has been written.  Usually needs to flush buffer.
//
// NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.

METHODDEF(void) term_destination(j_compress_ptr cinfo) {
  JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *)cinfo->dest;
  size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

  CUtlBuffer *buf = dest->pBuffer;

  /* Write any data remaining in the buffer */
  if (datacount > 0) {
    buf->Put(dest->buffer, datacount);
  }
}

// Purpose: Set up functions for writing data to a CUtlBuffer instead of FILE *

GLOBAL(void) jpeg_UtlBuffer_dest(j_compress_ptr cinfo, CUtlBuffer *pBuffer) {
  JPEGDestinationManager_t *dest;

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if (cinfo->dest == NULL) { /* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(JPEGDestinationManager_t));
  }

  dest = (JPEGDestinationManager_t *)cinfo->dest;

  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;
  dest->pBuffer = pBuffer;
}

// Purpose: Functionality shared by all video modes
class CVideoMode_Common : public IVideoMode {
 public:
  CVideoMode_Common() {
    m_nNumModes = 0;
    m_bInitialized = false;

    DefaultVideoMode().width = 640;
    DefaultVideoMode().height = 480;
    DefaultVideoMode().bpp = 32;
    DefaultVideoMode().refreshRate = 0;

    RequestedWindowVideoMode().width = -1;
    RequestedWindowVideoMode().height = -1;
    RequestedWindowVideoMode().bpp = 32;
    RequestedWindowVideoMode().refreshRate = 0;

    m_bClientViewRectDirty = false;
    m_pBackgroundTexture = NULL;
    m_pLoadingTexture = NULL;
    m_bWindowed = false;
    m_nModeWidth = IsPC() ? 1024 : 640;
    m_nModeHeight = IsPC() ? 768 : 480;
  }
  virtual ~CVideoMode_Common() {}

  // Methods of IVideoMode
  virtual bool Init() { return true; }
  virtual void Shutdown(void);
  // Returns the enumerated video mode
  virtual vmode_t *GetMode(int num) {
    if (num < 0) return &m_nCustomModeList[-num - 1];

    if (num >= m_nNumModes) return &DefaultVideoMode();

    return &m_rgModeList[num];
  }
  virtual int GetModeCount(void) { return m_nNumModes; }
  virtual bool IsWindowedMode(void) const { return m_bWindowed; }
  virtual void UpdateWindowPosition(void);
  virtual void RestoreVideo(void);
  virtual void ReleaseVideo(void);
  virtual void DrawNullBackground(void *vhdc, int w, int h) {
    if (IsX360()) return;

    HDC hdc = (HDC)vhdc;

    // Show a message if running without renderer..
    if (CommandLine()->FindParm("-noshaderapi")) {
      HFONT fnt = CreateFontA(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, DEFAULT_PITCH, "Arial");

      HFONT oldFont = (HFONT)SelectObject(hdc, fnt);
      int oldBkMode = SetBkMode(hdc, TRANSPARENT);
      COLORREF oldFgColor = SetTextColor(hdc, RGB(255, 255, 255));

      HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
      HBRUSH oldBr = (HBRUSH)SelectObject(hdc, br);
      Rectangle(hdc, 0, 0, w, h);

      RECT rc;
      rc.left = 0;
      rc.top = 0;
      rc.right = w;
      rc.bottom = h;

      DrawText(hdc, "Running with -noshaderapi", -1, &rc,
               DT_NOPREFIX | DT_VCENTER | DT_CENTER | DT_SINGLELINE);

      rc.top = rc.bottom - 30;

      if (host_state.worldmodel != NULL) {
        rc.left += 10;
        DrawText(hdc, modelloader->GetName(host_state.worldmodel), -1, &rc,
                 DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
      }

      SetTextColor(hdc, oldFgColor);

      SelectObject(hdc, oldBr);
      SetBkMode(hdc, oldBkMode);
      SelectObject(hdc, oldFont);

      DeleteObject(br);
      DeleteObject(fnt);
    }
  }
  virtual void InvalidateWindow() {
    if (CommandLine()->FindParm("-noshaderapi")) {
      InvalidateRect((HWND)game->GetMainWindow(), NULL, FALSE);
    }
  }
  virtual void DrawStartupGraphic() {
    SetupStartupGraphic();

    if (!m_pBackgroundTexture || !m_pLoadingTexture) return;

    CMatRenderContextPtr pRenderContext(g_pMaterialSystem);

    char pStartupGraphicName[SOURCE_MAX_PATH];
    ComputeStartupGraphicName(pStartupGraphicName, sizeof(pStartupGraphicName));

    // Allocate a white material
    KeyValues *pVMTKeyValues = new KeyValues("UnlitGeneric");
    pVMTKeyValues->SetString("$basetexture", pStartupGraphicName + 10);
    pVMTKeyValues->SetInt("$ignorez", 1);
    pVMTKeyValues->SetInt("$nofog", 1);
    pVMTKeyValues->SetInt("$no_fullbright", 1);
    pVMTKeyValues->SetInt("$nocull", 1);
    IMaterial *pMaterial =
        g_pMaterialSystem->CreateMaterial("__background", pVMTKeyValues);

    pVMTKeyValues = new KeyValues("UnlitGeneric");
    pVMTKeyValues->SetString("$basetexture", "console/startup_loading.vtf");
    pVMTKeyValues->SetInt("$translucent", 1);
    pVMTKeyValues->SetInt("$ignorez", 1);
    pVMTKeyValues->SetInt("$nofog", 1);
    pVMTKeyValues->SetInt("$no_fullbright", 1);
    pVMTKeyValues->SetInt("$nocull", 1);
    IMaterial *pLoadingMaterial =
        g_pMaterialSystem->CreateMaterial("__loading", pVMTKeyValues);

    int w = GetModeWidth();
    int h = GetModeHeight();
    int tw = m_pBackgroundTexture->Width();
    int th = m_pBackgroundTexture->Height();
    int lw = m_pLoadingTexture->Width();
    int lh = m_pLoadingTexture->Height();
    pRenderContext->Viewport(0, 0, w, h);
    pRenderContext->DepthRange(0, 1);
    pRenderContext->ClearColor3ub(0, 0, 0);
    pRenderContext->ClearBuffers(true, true, true);
    pRenderContext->SetToneMappingScaleLinear(Vector(1, 1, 1));
    DrawScreenSpaceRectangle(pMaterial, 0, 0, w, h, 0, 0, tw - 1, th - 1, tw,
                             th);
    DrawScreenSpaceRectangle(pLoadingMaterial, w - lw, h - lh, lw, lh, 0, 0,
                             lw - 1, lh - 1, lw, lh);
    g_pMaterialSystem->SwapBuffers();

    pMaterial->Release();
    pLoadingMaterial->Release();

    // release graphics
    DestroyVTFTexture(m_pBackgroundTexture);
    m_pBackgroundTexture = NULL;
    DestroyVTFTexture(m_pLoadingTexture);
    m_pLoadingTexture = NULL;
  }
  virtual bool CreateGameWindow(int nWidth, int nHeight, bool bWindowed) {
    Plat_TimestampedLog("Engine::CVideoMode_Common: CreateGameWindow");

    // This allows you to have a window of any size.
    // Requires you to set both width and height for the window and
    // that you start in windowed mode
    if (bWindowed && nWidth && nHeight) {
      // TODO(d.rattman): There's some ordering issues related to the config
      // record and reading the command-line. Would be nice for just one place
      // where this is done.
      RequestedWindowVideoMode().width = nWidth;
      RequestedWindowVideoMode().height = nHeight;
    }

    if (!InEditMode()) {
      // Fill in vid structure for the mode.
      // Note: ModeWidth/Height may *not* match requested nWidth/nHeight
      ResetCurrentModeForNewResolution(nWidth, nHeight, bWindowed);

      // When running in stand-alone mode, create your own window
      if (!game->CreateGameWindow()) return false;

      // Re-size and re-center the window
      AdjustWindow(GetModeWidth(), GetModeHeight(), GetModeBPP(),
                   IsWindowedMode());

      // Play our videos for the background
      DrawStartupVideo();

      // Set the mode and let the materialsystem take over
      if (!SetMode(GetModeWidth(), GetModeHeight(), IsWindowedMode()))
        return false;

      // Play our videos or display our temp image for the background
      DrawStartupGraphic();
    }

    return true;
  }
  // Returns the video mode width + height.
  virtual int GetModeWidth(void) const { return m_nModeWidth; }
  virtual int GetModeHeight(void) const { return m_nModeHeight; }
  virtual const vrect_t &GetClientViewRect() const;
  virtual void SetClientViewRect(const vrect_t &viewRect);
  virtual void MarkClientViewRectDirty();
  virtual void TakeSnapshotTGA(const char *pFileName);
  virtual void TakeSnapshotTGARect(const char *pFilename, int x, int y, int w,
                                   int h, int resampleWidth, int resampleHeight,
                                   bool bPFM, CubeMapFaceIndex_t faceIndex);
  virtual void WriteMovieFrame(const MovieInfo_t &info);
  virtual void TakeSnapshotJPEG(const char *pFileName, int quality) {
    Assert(pFileName);

    // Output buffer
    CUtlBuffer buf(0, 0);
    TakeSnapshotJPEGToBuffer(buf, quality);

    int finalSize = 0;
    FileHandle_t fh = g_pFileSystem->Open(pFileName, "wb");
    if (FILESYSTEM_INVALID_HANDLE != fh) {
      g_pFileSystem->Write(buf.Base(), buf.TellPut(), fh);
      finalSize = g_pFileSystem->Tell(fh);
      g_pFileSystem->Close(fh);
    }

    // Show info to console.
    char orig[64];
    char final[64];
    Q_strncpy(orig, Q_pretifymem(GetModeWidth() * 3 * GetModeHeight(), 2),
              sizeof(orig));
    Q_strncpy(final, Q_pretifymem(finalSize, 2), sizeof(final));

    Msg("Wrote '%s':  %s (%dx%d) compresssed (quality %i) to %s\n", pFileName,
        orig, GetModeWidth(), GetModeHeight(), quality, final);
  }
  virtual bool TakeSnapshotJPEGToBuffer(CUtlBuffer &buf, int quality) {
    if (g_LostVideoMemory) return false;

    // Validate quality level
    quality = std::clamp(quality, 1, 100);

    // Allocate space for bits
    uint8_t *pImage = new uint8_t[GetModeWidth() * 3 * GetModeHeight()];
    if (!pImage) {
      Msg("Unable to allocate %i bytes for image\n",
          GetModeWidth() * 3 * GetModeHeight());
      return false;
    }

    // Get Bits from the material system
    ReadScreenPixels(0, 0, GetModeWidth(), GetModeHeight(), pImage,
                     IMAGE_FORMAT_RGB888);

    JSAMPROW row_pointer[1];  // pointer to JSAMPLE row[s]
    int row_stride;           // physical row width in image buffer

    // stderr handler
    struct jpeg_error_mgr jerr;

    // compression data structure
    struct jpeg_compress_struct cinfo;

    row_stride = GetModeWidth() * 3;  // JSAMPLEs per row in image_buffer

    // point at stderr
    cinfo.err = jpeg_std_error(&jerr);

    // create compressor
    jpeg_create_compress(&cinfo);

    // Hook CUtlBuffer to compression
    jpeg_UtlBuffer_dest(&cinfo, &buf);

    // image width and height, in pixels
    cinfo.image_width = GetModeWidth();
    cinfo.image_height = GetModeHeight();
    // RGB is 3 componnent
    cinfo.input_components = 3;
    // # of color components per pixel
    cinfo.in_color_space = JCS_RGB;

    // Apply settings
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    // Start compressor
    jpeg_start_compress(&cinfo, TRUE);

    // Write scanlines
    while (cinfo.next_scanline < cinfo.image_height) {
      row_pointer[0] = &pImage[cinfo.next_scanline * row_stride];
      jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Finalize image
    jpeg_finish_compress(&cinfo);

    // Cleanup
    jpeg_destroy_compress(&cinfo);

    delete[] pImage;

    return true;
  }

 protected:
  bool GetInitialized() const { return m_bInitialized; }
  void SetInitialized(bool init) { m_bInitialized = init; }
  void AdjustWindow(int nWidth, int nHeight, int nBPP, bool bWindowed);
  void ResetCurrentModeForNewResolution(int width, int height, bool bWindowed) {
    // Fill in vid structure for the mode
    int nGameMode = FindVideoMode(width, height, bWindowed);
    vmode_t *pMode = GetMode(nGameMode);
    m_bWindowed = bWindowed;
    m_nModeWidth = pMode->width;
    m_nModeHeight = pMode->height;
  }
  int GetModeBPP() const { return 32; }
  void DrawStartupVideo() {
    // render an avi, if we have one
    if (!m_bPlayedStartupVideo && !InEditMode()) {
      game->PlayStartupVideos();
      m_bPlayedStartupVideo = true;
    }
  }
  void ComputeStartupGraphicName(char *pBuf, int nBufLen) {
    char szBackgroundName[SOURCE_MAX_PATH];
    CL_GetBackgroundLevelName(szBackgroundName, sizeof(szBackgroundName),
                              false);

    float aspectRatio = (float)GetModeWidth() / GetModeHeight();
    if (aspectRatio >= 1.6f) {
      // use the widescreen version
      Q_snprintf(pBuf, nBufLen, "materials/console/%s_widescreen.vtf",
                 szBackgroundName);
    } else {
      Q_snprintf(pBuf, nBufLen, "materials/console/%s.vtf", szBackgroundName);
    }

    if (!g_pFileSystem->FileExists(pBuf, "GAME")) {
      Q_strncpy(pBuf,
                (aspectRatio >= 1.6f)
                    ? "materials/console/background01_widescreen.vtf"
                    : "materials/console/background01.vtf",
                nBufLen);
    }
  }

  // Finds the video mode in the list of video modes
  // Finds the video mode in the list of video modes
  int FindVideoMode(int nDesiredWidth, int nDesiredHeight, bool bWindowed) {
    // Check the default window size..
    if ((nDesiredWidth == DefaultVideoMode().width) &&
        (nDesiredHeight == DefaultVideoMode().height))
      return VIDEO_MODE_DEFAULT;

    // Check the requested window size, but only if we're running windowed
    if (bWindowed) {
      if ((nDesiredWidth == RequestedWindowVideoMode().width) &&
          (nDesiredHeight == RequestedWindowVideoMode().height))
        return VIDEO_MODE_REQUESTED_WINDOW_SIZE;
    }

    int i;
    int iOK = VIDEO_MODE_DEFAULT;
    for (i = 0; i < m_nNumModes; i++) {
      // Match width first
      if (m_rgModeList[i].width != nDesiredWidth) continue;

      iOK = i;

      if (m_rgModeList[i].height != nDesiredHeight) continue;

      // Found a decent match
      break;
    }

    // No match, use mode 0
    if (i >= m_nNumModes) {
      if (iOK != VIDEO_MODE_DEFAULT) {
        i = iOK;
      } else {
        i = 0;
      }
    }

    return i;
  }

  // Purpose: Returns the optimal refresh rate for the specified mode
  int GetRefreshRateForMode(const vmode_t *pMode);

  // Inline accessors
  vmode_t &DefaultVideoMode() {
    return m_nCustomModeList[-VIDEO_MODE_DEFAULT - 1];
  }
  vmode_t &RequestedWindowVideoMode() {
    return m_nCustomModeList[-VIDEO_MODE_REQUESTED_WINDOW_SIZE - 1];
  }

 private:
  // Purpose: Loads the startup graphic
  void SetupStartupGraphic() {
    Plat_TimestampedLog("Engine::CVideoMode_Common: SetupStartupGraphic");

    char szBackgroundName[SOURCE_MAX_PATH];
    CL_GetBackgroundLevelName(szBackgroundName, sizeof(szBackgroundName),
                              false);

    // get the image to load
    char material[SOURCE_MAX_PATH];
    CUtlBuffer buf;

    float aspectRatio = (float)GetModeWidth() / GetModeHeight();
    if (aspectRatio >= 1.6f) {
      // use the widescreen version
      Q_snprintf(material, sizeof(material),
                 "materials/console/%s_widescreen.vtf", szBackgroundName);
    } else {
      Q_snprintf(material, sizeof(material), "materials/console/%s.vtf",
                 szBackgroundName);
    }

    // load in the background vtf
    m_pBackgroundTexture = LoadVTF(buf, material);
    if (!m_pBackgroundTexture) {
      // fallback to opening just the default background
      m_pBackgroundTexture =
          LoadVTF(buf, (aspectRatio >= 1.6f)
                           ? "materials/console/background01_widescreen.vtf"
                           : "materials/console/background01.vtf");
      if (!m_pBackgroundTexture) {
        Error("Can't find background image '%s'\n", material);
        return;
      }
    }

    // loading.vtf
    m_pLoadingTexture = LoadVTF(buf, "materials/console/startup_loading.vtf");
    if (!m_pLoadingTexture) {
      Error(
          "Can't find background image "
          "materials/console/startup_loading.vtf\n");
      return;
    }
  }
  void CenterEngineWindow(HWND hWndCenter, int width, int height);
  void DrawStartupGraphic(HWND window);
  void BlitGraphicToHDC(HDC hdc, uint8_t *rgba, int imageWidth, int imageHeight,
                        int x0, int y0, int x1, int y1);
  void BlitGraphicToHDCWithAlpha(HDC hdc, uint8_t *rgba, int imageWidth,
                                 int imageHeight, int x0, int y0, int x1,
                                 int y1) {
    int x = x0;
    int y = y0;
    int wide = x1 - x0;
    int tall = y1 - y0;

    Assert(imageWidth == wide && imageHeight == tall);

    int texwby4 = imageWidth << 2;

    for (int v = 0; v < tall; v++) {
      int *src = (int *)(rgba + (v * texwby4));
      int xaccum = 0;

      for (int u = 0; u < wide; u++) {
        uint8_t *xsrc = (uint8_t *)(src + xaccum);
        if (xsrc[3]) {
          ::SetPixel(hdc, x + u, y + v, RGB(xsrc[0], xsrc[1], xsrc[2]));
        }
        xaccum += 1;
      }
    }
  }
  IVTFTexture *LoadVTF(CUtlBuffer &temp, const char *szFileName) {
    if (!g_pFileSystem->ReadFile(szFileName, NULL, temp)) return NULL;

    IVTFTexture *texture = CreateVTFTexture();
    if (!texture->Unserialize(temp)) {
      Error("Invalid or corrupt background texture %s\n", szFileName);
      return nullptr;
    }

    texture->ConvertImageFormat(IMAGE_FORMAT_RGBA8888, false);
    return texture;
  }
  void RecomputeClientViewRect();

  // Overridden by derived classes
  virtual void ReleaseFullScreen(void);
  virtual void ChangeDisplaySettingsToFullscreen(int nWidth, int nHeight,
                                                 int nBPP);
  virtual void ReadScreenPixels(int x, int y, int w, int h, void *pBuffer,
                                ImageFormat format);

  // PFM screenshot methods
  ITexture *GetBuildCubemaps16BitTexture(void);
  ITexture *GetFullFrameFB0(void);

  void BlitHiLoScreenBuffersTo16Bit(void);
  void TakeSnapshotPFMRect(const char *pFilename, int x, int y, int w, int h,
                           int resampleWidth, int resampleHeight,
                           CubeMapFaceIndex_t faceIndex);

 protected:
  enum { MAX_MODE_LIST = 512 };

  enum {
    VIDEO_MODE_DEFAULT = -1,
    VIDEO_MODE_REQUESTED_WINDOW_SIZE = -2,
    CUSTOM_VIDEO_MODES = 2
  };

  // Master mode list
  int m_nNumModes;
  vmode_t m_rgModeList[MAX_MODE_LIST];
  vmode_t m_nCustomModeList[CUSTOM_VIDEO_MODES];
  bool m_bInitialized;
  bool m_bPlayedStartupVideo;

  // Renderable surface information
  int m_nModeWidth;
  int m_nModeHeight;
  bool m_bWindowed;
  bool m_bSetModeOnce;

  // Client view rectangle
  vrect_t m_ClientViewRect;
  bool m_bClientViewRectDirty;

  // loading image
  IVTFTexture *m_pBackgroundTexture;
  IVTFTexture *m_pLoadingTexture;
};

static int __cdecl VideoModeCompare(const void *arg1, const void *arg2) {
  vmode_t *m1 = (vmode_t *)arg1, *m2 = (vmode_t *)arg2;

  if (m1->width < m2->width) {
    return -1;
  }

  if (m1->width == m2->width) {
    if (m1->height < m2->height) {
      return -1;
    }

    if (m1->height > m2->height) {
      return 1;
    }

    return 0;
  }

  return 1;
}

// Purpose: Renders the startup video into the HWND

// Purpose: Renders the startup graphic into the HWND

// Purpose: Blits an image to the loading window hdc

#ifndef _WIN32

typedef unsigned char BYTE;
typedef signed long LONG;
typedef unsigned long ULONG;

typedef char *LPSTR;

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG biWidth;
  LONG biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG biXPelsPerMeter;
  LONG biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER {
  WORD bfType;
  DWORD bfSize;
  WORD bfReserved1;
  WORD bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;

/* constants for the biCompression field */
#define BI_RGB 0L
#define BI_RLE8 1L
#define BI_RLE4 2L
#define BI_BITFIELDS 3L

typedef struct _GUID {
  unsigned long Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char Data4[8];
} GUID;

typedef GUID UUID;
#endif

// Purpose: Blits an image to the loading window hdc

void CVideoMode_Common::BlitGraphicToHDC(HDC hdc, uint8_t *rgba, int imageWidth,
                                         int imageHeight, int x0, int y0,
                                         int x1, int y1) {
  int x = x0;
  int y = y0;
  int wide = x1 - x0;
  int tall = y1 - y0;

  // Needs to be a multiple of 4
  int dibwide = (wide + 3) & ~3;

  Assert(rgba);
  int texwby4 = imageWidth << 2;

  double st = Plat_FloatTime();

  void *destBits = NULL;

  HBITMAP bm;
  BITMAPINFO bmi;
  Q_memset(&bmi, 0, sizeof(bmi));

  BITMAPINFOHEADER *hdr = &bmi.bmiHeader;

  hdr->biSize = sizeof(*hdr);
  hdr->biWidth = dibwide;
  hdr->biHeight = -tall;  // top down bitmap
  hdr->biBitCount = 24;
  hdr->biPlanes = 1;
  hdr->biCompression = BI_RGB;
  hdr->biSizeImage = dibwide * tall * 3;
  hdr->biXPelsPerMeter = 3780;
  hdr->biYPelsPerMeter = 3780;

  // Create a "source" DC
  HDC tempDC = CreateCompatibleDC(hdc);

  // Create the dibsection bitmap
  bm = CreateDIBSection(tempDC,          // handle to DC
                        &bmi,            // bitmap data
                        DIB_RGB_COLORS,  // data type indicator
                        &destBits,       // bit values
                        NULL,            // handle to file mapping object
                        0                // offset to bitmap bit values
  );

  // Select it into the source DC
  HBITMAP oldBitmap = (HBITMAP)SelectObject(tempDC, bm);

  // Setup for bilinaer filtering. If we don't do this filter here, there will
  // be a big annoying pop when it switches to the vguimatsurface version of the
  // background. We leave room for 14 bits of integer precision, so the image
  // can be up to 16k x 16k.
  const int BILINEAR_FIX_SHIFT = 17;
  const int BILINEAR_FIX_MUL = (1 << BILINEAR_FIX_SHIFT);

#define FIXED_BLEND(a, b, out, frac)                                          \
  out[0] =                                                                    \
      (a[0] * frac + b[0] * (BILINEAR_FIX_MUL - frac)) >> BILINEAR_FIX_SHIFT; \
  out[1] =                                                                    \
      (a[1] * frac + b[1] * (BILINEAR_FIX_MUL - frac)) >> BILINEAR_FIX_SHIFT; \
  out[2] =                                                                    \
      (a[2] * frac + b[2] * (BILINEAR_FIX_MUL - frac)) >> BILINEAR_FIX_SHIFT;

  float eps = 0.001f;
  float uMax = imageWidth - 1 - eps;
  float vMax = imageHeight - 1 - eps;

  int fixedBilinearV = 0;
  int bilinearUInc = (int)((uMax / (dibwide - 1)) * BILINEAR_FIX_MUL);
  int bilinearVInc = (int)((vMax / (tall - 1)) * BILINEAR_FIX_MUL);

  for (int v = 0; v < tall; v++) {
    int iBilinearV = fixedBilinearV >> BILINEAR_FIX_SHIFT;
    int fixedFractionV = fixedBilinearV & (BILINEAR_FIX_MUL - 1);
    fixedBilinearV += bilinearVInc;

    int fixedBilinearU = 0;
    uint8_t *dest = (uint8_t *)destBits + ((y + v) * dibwide + x) * 3;

    for (int u = 0; u < dibwide; u++, dest += 3) {
      int iBilinearU = fixedBilinearU >> BILINEAR_FIX_SHIFT;
      int fixedFractionU = fixedBilinearU & (BILINEAR_FIX_MUL - 1);
      fixedBilinearU += bilinearUInc;

      Assert(iBilinearU >= 0 && iBilinearU + 1 < imageWidth);
      Assert(iBilinearV >= 0 && iBilinearV + 1 < imageHeight);

      uint8_t *srcTopLine = rgba + iBilinearV * texwby4;
      uint8_t *srcBottomLine = rgba + (iBilinearV + 1) * texwby4;

      uint8_t *xsrc[4] = {srcTopLine + (iBilinearU + 0) * 4,
                          srcTopLine + (iBilinearU + 1) * 4,
                          srcBottomLine + (iBilinearU + 0) * 4,
                          srcBottomLine + (iBilinearU + 1) * 4};

      int topColor[3], bottomColor[3], finalColor[3];
      FIXED_BLEND(xsrc[1], xsrc[0], topColor, fixedFractionU);
      FIXED_BLEND(xsrc[3], xsrc[2], bottomColor, fixedFractionU);
      FIXED_BLEND(bottomColor, topColor, finalColor, fixedFractionV);

      // Windows wants the colors in reverse order.
      dest[0] = finalColor[2];
      dest[1] = finalColor[1];
      dest[2] = finalColor[0];
    }
  }

  // Now do the Blt
  BitBlt(hdc, 0, 0, dibwide, tall, tempDC, 0, 0, SRCCOPY);

  // This only draws if running -noshaderapi
  DrawNullBackground(hdc, dibwide, tall);

  // Restore the old Bitmap
  SelectObject(tempDC, oldBitmap);

  // Destroy the temporary DC
  DeleteDC(tempDC);

  // Destroy the DIBSection bitmap
  DeleteObject(bm);

  double elapsed = Plat_FloatTime() - st;

  Plat_TimestampedLog("Engine::CVideoMode_Common: BlitGraphicToHDC (%.4f s).",
                      elapsed);
}

// Purpose: This is called in response to a WM_MOVE message

void CVideoMode_Common::UpdateWindowPosition(void) {
  // Get the window from the game ( right place for it? )
  auto [x, y, w, h] = game->GetWindowRect();

  RECT window_rect;
  window_rect.left = x;
  window_rect.right = x + w;
  window_rect.top = y;
  window_rect.bottom = y + h;

  // NOTE: We need to feed this back into the video mode stuff
  // esp. in Resizing window mode.
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen(int nWidth,
                                                          int nHeight,
                                                          int nBPP) {}

void CVideoMode_Common::ReleaseFullScreen(void) {}

// Purpose: Returns the optimal refresh rate for the specified mode

int CVideoMode_Common::GetRefreshRateForMode(const vmode_t *pMode) {
  int nRefreshRate = pMode->refreshRate;

  // TODO(d.rattman): We should only read this once, at the beginning
  // override the refresh rate from the command-line maybe
  nRefreshRate = CommandLine()->ParmValue("-freq", nRefreshRate);
  nRefreshRate = CommandLine()->ParmValue("-refresh", nRefreshRate);
  nRefreshRate = CommandLine()->ParmValue("-refreshrate", nRefreshRate);

  return nRefreshRate;
}

// Purpose:
// Input  : *mode -
// Output : Returns true on success, false on failure.

void CVideoMode_Common::AdjustWindow(int nWidth, int nHeight, int nBPP,
                                     bool bWindowed) {
  if (g_bTextMode) return;

  // Use Change Display Settings to go full screen
  ChangeDisplaySettingsToFullscreen(nWidth, nHeight, nBPP);

  RECT WindowRect;
  WindowRect.top = 0;
  WindowRect.left = 0;
  WindowRect.right = nWidth;
  WindowRect.bottom = nHeight;

#ifndef _X360
  // Get window style
  DWORD style = GetWindowLongPtr((HWND)game->GetMainWindow(), GWL_STYLE);
  DWORD exStyle = GetWindowLongPtr((HWND)game->GetMainWindow(), GWL_EXSTYLE);

  if (bWindowed) {
    // Give it a frame (pretty much WS_OVERLAPPEDWINDOW except for we do not
    // modify the flags corresponding to resizing-frame and maximize-box)
    style |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    SetWindowLongPtr((HWND)game->GetMainWindow(), GWL_STYLE, style);

    // remove topmost flag
    exStyle &= ~WS_EX_TOPMOST;
    SetWindowLongPtr((HWND)game->GetMainWindow(), GWL_EXSTYLE, exStyle);
  }

  // Compute rect needed for that size client area based on window style
  AdjustWindowRectEx(&WindowRect, style, FALSE, exStyle);
#endif

  // Prepare to set window pos, which is required when toggling between topmost
  // and not window flags
  HWND hWndAfter = NULL;
  DWORD dwSwpFlags = 0;
#ifndef _X360
  {
    if (bWindowed) {
      hWndAfter = HWND_NOTOPMOST;
    } else {
      hWndAfter = HWND_TOPMOST;
    }
    dwSwpFlags = SWP_FRAMECHANGED;
  }
#else
  { dwSwpFlags = SWP_NOZORDER; }
#endif

  // Move the window to 0, 0 and the new true size
  SetWindowPos((HWND)game->GetMainWindow(), hWndAfter, 0, 0,
               WindowRect.right - WindowRect.left,
               WindowRect.bottom - WindowRect.top, SWP_NOREDRAW | dwSwpFlags);

  // Now center
  CenterEngineWindow((HWND)game->GetMainWindow(),
                     WindowRect.right - WindowRect.left,
                     WindowRect.bottom - WindowRect.top);
  game->SetWindowSize(nWidth, nHeight);

  // Make sure we have updated window information
  UpdateWindowPosition();
  MarkClientViewRectDirty();
}

// Purpose:

void CVideoMode_Common::Shutdown(void) {
  ReleaseFullScreen();
  game->DestroyGameWindow();

  if (!GetInitialized()) return;

  SetInitialized(false);
}

// Gets/sets the client view rectangle

const vrect_t &CVideoMode_Common::GetClientViewRect() const {
  const_cast<CVideoMode_Common *>(this)->RecomputeClientViewRect();
  return m_ClientViewRect;
}

void CVideoMode_Common::SetClientViewRect(const vrect_t &viewRect) {
  m_ClientViewRect = viewRect;
}

// Marks the client view rect dirty

void CVideoMode_Common::MarkClientViewRectDirty() {
  m_bClientViewRectDirty = true;
}

void CVideoMode_Common::RecomputeClientViewRect() {
  if (!InEditMode()) {
    if (!m_bClientViewRectDirty) return;
  }

  m_bClientViewRectDirty = false;

  int nWidth, nHeight;
  CMatRenderContextPtr pRenderContext(materials);

  pRenderContext->GetRenderTargetDimensions(nWidth, nHeight);
  m_ClientViewRect.width = nWidth;
  m_ClientViewRect.height = nHeight;
  m_ClientViewRect.x = 0;
  m_ClientViewRect.y = 0;

  if (!nWidth || !nHeight) {
    // didn't successfully get the screen size, try again next frame
    // window is probably minimized
    m_bClientViewRectDirty = true;
  }
}

// Purpose:
// Input  : hWndCenter -
//			width -
//			height -
// Output : static void

void CVideoMode_Common::CenterEngineWindow(HWND hWndCenter, int width,
                                           int height) {
  // In windowed mode go through game->GetDesktopInfo because system metrics
  // change when going fullscreen vs windowed. Use system metrics for
  // fullscreen or when game didn't have a chance to initialize.

  int cxScreen = 0, cyScreen = 0, refreshRate = 0;

  if (!(WS_EX_TOPMOST & ::GetWindowLong(hWndCenter, GWL_EXSTYLE))) {
    std::tie(cxScreen, cyScreen, refreshRate) = game->GetDesktopInfo();
  }

  if (!cxScreen || !cyScreen) {
    cxScreen = GetSystemMetrics(SM_CXSCREEN);
    cyScreen = GetSystemMetrics(SM_CYSCREEN);
  }

  // Compute top-left corner offset
  int CenterX = (cxScreen - width) / 2;
  int CenterY = (cyScreen - height) / 2;
  CenterX = (CenterX < 0) ? 0 : CenterX;
  CenterY = (CenterY < 0) ? 0 : CenterY;

  // tweak the x and w positions if the user species them on the command-line
  CenterX = CommandLine()->ParmValue("-x", CenterX);
  CenterY = CommandLine()->ParmValue("-y", CenterY);

  game->SetWindowXY(CenterX, CenterY);

  SetWindowPos(hWndCenter, NULL, CenterX, CenterY, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

// Handle alt-tab

void CVideoMode_Common::RestoreVideo() {}

void CVideoMode_Common::ReleaseVideo() {}

// Read screen pixels

void CVideoMode_Common::ReadScreenPixels(int x, int y, int w, int h,
                                         void *pBuffer, ImageFormat format) {
  int nBytes = ImageLoader::GetMemRequired(w, h, 1, format, false);
  memset(pBuffer, 0, nBytes);
}

// Purpose: Write vid.buffer out as a .tga file

void CVideoMode_Common::TakeSnapshotTGA(const char *pFilename) {
  // bitmap bits
  uint8_t *pImage = new uint8_t[GetModeWidth() * 3 * GetModeHeight()];

  // Get Bits from the material system
  ReadScreenPixels(0, 0, GetModeWidth(), GetModeHeight(), pImage,
                   IMAGE_FORMAT_RGB888);

  CUtlBuffer outBuf;
  if (TGAWriter::WriteToBuffer(pImage, outBuf, GetModeWidth(), GetModeHeight(),
                               IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888)) {
    if (!g_pFileSystem->WriteFile(pFilename, NULL, outBuf)) {
      Warning("Couldn't write bitmap data snapshot to file %s.\n", pFilename);
    }
  }

  delete[] pImage;
}

// PFM screenshot helpers

ITexture *CVideoMode_Common::GetBuildCubemaps16BitTexture(void) {
  return materials->FindTexture("_rt_BuildCubemaps16bit",
                                TEXTURE_GROUP_RENDER_TARGET);
}

ITexture *CVideoMode_Common::GetFullFrameFB0(void) {
  return materials->FindTexture("_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET);
}

void CVideoMode_Common::BlitHiLoScreenBuffersTo16Bit(void) {
  IMaterial *pHDRCombineMaterial = materials->FindMaterial(
      "dev/hdrcombineto16bit", TEXTURE_GROUP_OTHER, true);
  //	if( IsErrorMaterial( pHDRCombineMaterial ) )
  //	{
  //		Assert( 0 );
  //		return;
  //	}

  CMatRenderContextPtr pRenderContext(materials);
  ITexture *pSaveRenderTarget;
  pSaveRenderTarget = pRenderContext->GetRenderTarget();

  int oldX, oldY, oldW, oldH;
  pRenderContext->GetViewport(oldX, oldY, oldW, oldH);

  pRenderContext->SetRenderTarget(GetBuildCubemaps16BitTexture());
  int width, height;
  pRenderContext->GetRenderTargetDimensions(width, height);
  pRenderContext->Viewport(0, 0, width, height);
  pRenderContext->DrawScreenSpaceQuad(pHDRCombineMaterial);

  pRenderContext->SetRenderTarget(pSaveRenderTarget);
  pRenderContext->Viewport(oldX, oldY, oldW, oldH);
}

void GetCubemapOffset(CubeMapFaceIndex_t faceIndex, int &x, int &y,
                      int &faceDim) {
  int fbWidth, fbHeight;
  materials->GetBackBufferDimensions(fbWidth, fbHeight);

  if (fbWidth * 4 > fbHeight * 3) {
    faceDim = fbHeight / 3;
  } else {
    faceDim = fbWidth / 4;
  }

  switch (faceIndex) {
    case CUBEMAP_FACE_RIGHT:
      x = 2;
      y = 1;
      break;
    case CUBEMAP_FACE_LEFT:
      x = 0;
      y = 1;
      break;
    case CUBEMAP_FACE_BACK:
      x = 1;
      y = 1;
      break;
    case CUBEMAP_FACE_FRONT:
      x = 3;
      y = 1;
      break;
    case CUBEMAP_FACE_UP:
      x = 2;
      y = 0;
      break;
    case CUBEMAP_FACE_DOWN:
      x = 2;
      y = 2;
      break;
    default:
      SOURCE_UNREACHABLE();
  }
  x *= faceDim;
  y *= faceDim;
}

// Takes a snapshot	to PFM

void CVideoMode_Common::TakeSnapshotPFMRect(const char *pFilename, int x, int y,
                                            int w, int h, int resampleWidth,
                                            int resampleHeight,
                                            CubeMapFaceIndex_t faceIndex) {
  if (g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE) {
    Warning("Unable to take PFM screenshots if HDR isn't enabled!\n");
    return;
  }

  // hack
  //	resampleWidth = w;
  //	resampleHeight = h;
  // bitmap bits
  float16 *pImage = (float16 *)malloc(
      w * h * ImageLoader::SizeInBytes(IMAGE_FORMAT_RGBA16161616F));
  float *pImage1 = (float *)malloc(
      w * h * ImageLoader::SizeInBytes(IMAGE_FORMAT_RGB323232F));

  CMatRenderContextPtr pRenderContext(materials);

  // Save the current render target.
  ITexture *pSaveRenderTarget = pRenderContext->GetRenderTarget();

  // Set this as the render target so that we can read it.
  pRenderContext->SetRenderTarget(GetFullFrameFB0());

  // Get Bits from the material system
  ReadScreenPixels(x, y, w, h, pImage, IMAGE_FORMAT_RGBA16161616F);

  // Draw what we just grabbed to the screen
  pRenderContext->SetRenderTarget(NULL);

  int scrw, scrh;
  pRenderContext->GetRenderTargetDimensions(scrw, scrh);
  pRenderContext->Viewport(0, 0, scrw, scrh);

  int offsetX, offsetY, faceDim;
  GetCubemapOffset(faceIndex, offsetX, offsetY, faceDim);
  pRenderContext->DrawScreenSpaceRectangle(
      materials->FindMaterial("dev/copyfullframefb", ""), offsetX, offsetY,
      faceDim, faceDim, 0, 0, w - 1, h - 1, scrw, scrh);

  // Restore the render target.
  pRenderContext->SetRenderTarget(pSaveRenderTarget);

  // convert from float16 to float32
  ImageLoader::ConvertImageFormat(
      (unsigned char *)pImage, IMAGE_FORMAT_RGBA16161616F,
      (unsigned char *)pImage1, IMAGE_FORMAT_RGB323232F, w, h);

  Assert(w == h);  // garymcthack - this only works for square images

  float *pFloatImage =
      (float *)malloc(resampleWidth * resampleHeight *
                      ImageLoader::SizeInBytes(IMAGE_FORMAT_RGB323232F));

  ImageLoader::ResampleInfo_t info;
  info.m_pSrc = (unsigned char *)pImage1;
  info.m_pDest = (unsigned char *)pFloatImage;
  info.m_nSrcWidth = w;
  info.m_nSrcHeight = h;
  info.m_nDestWidth = resampleWidth;
  info.m_nDestHeight = resampleHeight;
  info.m_flSrcGamma = 1.0f;
  info.m_flDestGamma = 1.0f;

  if (!ImageLoader::ResampleRGB323232F(info)) {
    Sys_Error("Can't resample\n");
  }

  PFMWrite(pFloatImage, pFilename, resampleWidth, resampleHeight);

  free(pImage1);
  free(pImage);
  free(pFloatImage);
}

// Takes a snapshot

void CVideoMode_Common::TakeSnapshotTGARect(const char *pFilename, int x, int y,
                                            int w, int h, int resampleWidth,
                                            int resampleHeight, bool bPFM,
                                            CubeMapFaceIndex_t faceIndex) {
  if (bPFM) {
    TakeSnapshotPFMRect(pFilename, x, y, w, h, resampleWidth, resampleHeight,
                        faceIndex);
    return;
  }

  // bitmap bits
  uint8_t *pImage = new uint8_t[w * h * 4];
  uint8_t *pImage1 = new uint8_t[resampleWidth * resampleHeight * 4];

  // Get Bits from the material system
  ReadScreenPixels(x, y, w, h, pImage, IMAGE_FORMAT_RGBA8888);

  Assert(w == h);  // garymcthack - this only works for square images

  ImageLoader::ResampleInfo_t info;
  info.m_pSrc = pImage;
  info.m_pDest = pImage1;
  info.m_nSrcWidth = w;
  info.m_nSrcHeight = h;
  info.m_nDestWidth = resampleWidth;
  info.m_nDestHeight = resampleHeight;
  info.m_flSrcGamma = 1.0f;
  info.m_flDestGamma = 1.0f;

  if (!ImageLoader::ResampleRGBA8888(info)) {
    Sys_Error("Can't resample\n");
  }

  CUtlBuffer outBuf;
  if (TGAWriter::WriteToBuffer(pImage1, outBuf, resampleWidth, resampleHeight,
                               IMAGE_FORMAT_RGBA8888, IMAGE_FORMAT_RGBA8888)) {
    if (!g_pFileSystem->WriteFile(pFilename, NULL, outBuf)) {
      Error("Couldn't write bitmap data snapshot to file %s.\n", pFilename);
    }
  }

  delete[] pImage1;
  delete[] pImage;
  materials->SwapBuffers();
}

// Purpose: Writes the data in *data to the sequentially number .bmp file
// filename Input  : *filename -
//			width -
//			height -
//			depth -
//			*data -
// Output : static void

static void VID_ProcessMovieFrame(const MovieInfo_t &info, bool jpeg,
                                  const char *filename, int width, int height,
                                  uint8_t *data) {
  CUtlBuffer outBuf;
  bool bSuccess = false;
  if (jpeg) {
    bSuccess = videomode->TakeSnapshotJPEGToBuffer(outBuf, info.jpeg_quality);
  } else {
    bSuccess = TGAWriter::WriteToBuffer(
        data, outBuf, width, height, IMAGE_FORMAT_BGR888, IMAGE_FORMAT_RGB888);
  }

  if (bSuccess) {
    if (!g_pFileSystem->WriteFile(filename, NULL, outBuf)) {
      Warning("Couldn't write movie snapshot to file %s.\n", filename);
      Cbuf_AddText("endmovie\n");
    }
  }
}

// Purpose: Store current frame to numbered .bmp file
// Input  : *pFilename -

void CVideoMode_Common::WriteMovieFrame(const MovieInfo_t &info) {
  char const *pMovieName = info.moviename;
  int nMovieFrame = info.movieframe;

  if (g_LostVideoMemory) return;

  if (!pMovieName[0]) {
    Cbuf_AddText("endmovie\n");
    ConMsg("Tried to write movie buffer with no filename set!\n");
    return;
  }

  int imagesize = GetModeWidth() * GetModeHeight();
  BGR888_t *hp = new BGR888_t[imagesize];
  if (hp == NULL) {
    Sys_Error("Couldn't allocate bitmap header to snapshot.\n");
  }

  // Get Bits from material system
  ReadScreenPixels(0, 0, GetModeWidth(), GetModeHeight(), hp,
                   IMAGE_FORMAT_BGR888);

  // Store frame to disk
  if (info.DoTga()) {
    VID_ProcessMovieFrame(info, false,
                          va("%s%04d.tga", pMovieName, nMovieFrame),
                          GetModeWidth(), GetModeHeight(), (unsigned char *)hp);
  }

  if (info.DoJpg()) {
    VID_ProcessMovieFrame(info, true, va("%s%04d.jpg", pMovieName, nMovieFrame),
                          GetModeWidth(), GetModeHeight(), (unsigned char *)hp);
  }

  if (info.DoAVI()) {
    avi->AppendMovieFrame(g_hCurrentAVI, hp);
  }

  delete[] hp;
}

void VideoMode_AdjustForModeChange();

// The version of the VideoMode class for the material system
class CVideoMode_MaterialSystem : public CVideoMode_Common {
 public:
  using BaseClass = CVideoMode_Common;

  CVideoMode_MaterialSystem() {}

  virtual bool Init() {
    m_bSetModeOnce = false;
    m_bPlayedStartupVideo = false;

    // we only support 32-bit rendering.
    int bitsperpixel = 32;

    bool bAllowSmallModes = false;
    if (CommandLine()->FindParm("-small")) {
      bAllowSmallModes = true;
    }

    int nAdapter = materials->GetCurrentAdapter();
    int nModeCount = materials->GetModeCount(nAdapter);

    int nDesktopRefresh;
    std::tie(std::ignore, std::ignore, nDesktopRefresh) =
        game->GetDesktopInfo();

    for (int i = 0; i < nModeCount; i++) {
      MaterialVideoMode_t info;
      materials->GetModeInfo(nAdapter, i, info);

      if (info.m_Width < 640 || info.m_Height < 480) {
        if (!bAllowSmallModes) continue;
      }

      // make sure we don't already have this mode listed
      bool bAlreadyInList = false;
      for (int j = 0; j < m_nNumModes; j++) {
        if (info.m_Width == m_rgModeList[j].width &&
            info.m_Height == m_rgModeList[j].height) {
          // choose the highest refresh rate available for each mode up to the
          // desktop rate

          // if the new mode is valid and current is invalid or not as high,
          // choose the new one
          if (info.m_RefreshRate <= nDesktopRefresh &&
              (m_rgModeList[j].refreshRate > nDesktopRefresh ||
               m_rgModeList[j].refreshRate < info.m_RefreshRate)) {
            m_rgModeList[j].refreshRate = info.m_RefreshRate;
          }
          bAlreadyInList = true;
          break;
        }
      }

      if (bAlreadyInList) continue;

      m_rgModeList[m_nNumModes].width = info.m_Width;
      m_rgModeList[m_nNumModes].height = info.m_Height;
      m_rgModeList[m_nNumModes].bpp = bitsperpixel;
      // NOTE: Don't clamp this to the desktop rate because we want to be sure
      // we've only added modes that the adapter can do and maybe the desktop
      // rate isn't available in this mode
      m_rgModeList[m_nNumModes].refreshRate = info.m_RefreshRate;

      if (++m_nNumModes >= MAX_MODE_LIST) break;
    }

    // Sort modes for easy searching later
    if (m_nNumModes > 1) {
      qsort((void *)&m_rgModeList[0], m_nNumModes, sizeof(vmode_t),
            VideoModeCompare);
    }

    materials->AddModeChangeCallBack(&VideoMode_AdjustForModeChange);
    SetInitialized(true);
    return true;
  }
  virtual void Shutdown(void) {
    materials->RemoveModeChangeCallBack(&VideoMode_AdjustForModeChange);
    BaseClass::Shutdown();
  }
  virtual void SetGameWindow(void *hWnd) {
    if (hWnd == NULL) {
      // No longer confine rendering into this view
      materials->SetView(NULL);
      return;
    }

    // When running in edit mode, just use hammer's window
    game->SetGameWindow((HWND)hWnd);

    // TODO(d.rattman): Move this code into the _MaterialSystem version of
    // CVideoMode In editor mode, the mode width + height is equal to the
    // desktop width + height
    MaterialVideoMode_t mode;
    materials->GetDisplayMode(mode);
    m_bWindowed = true;
    m_nModeWidth = mode.m_Width;
    m_nModeHeight = mode.m_Height;

    materials->SetView(game->GetMainWindow());
  }
  virtual bool SetMode(int nWidth, int nHeight, bool bWindowed) {
    // Necessary for mode selection to work
    int nFoundMode = FindVideoMode(nWidth, nHeight, bWindowed);
    vmode_t *pMode = GetMode(nFoundMode);

    // update current video state
    MaterialSystem_Config_t config = *g_pMaterialSystemConfig;
    config.m_VideoMode.m_Width = pMode->width;
    config.m_VideoMode.m_Height = pMode->height;

#ifdef SWDS
    config.m_VideoMode.m_RefreshRate = 60;
#else
    config.m_VideoMode.m_RefreshRate = GetRefreshRateForMode(pMode);
#endif

    config.SetFlag(MATSYS_VIDCFG_FLAGS_WINDOWED, bWindowed);

    // TODO(d.rattman): This is trash. We have to do *different* things
    // depending on how we're setting the mode!
    if (!m_bSetModeOnce) {
      if (!materials->SetMode((void *)game->GetMainWindow(), config))
        return false;

      m_bSetModeOnce = true;

      InitStartupScreen();
      return true;
    }

    // update the config
    OverrideMaterialSystemConfig(config);
    return true;
  }
  virtual void ReleaseVideo(void) {
    if (IsWindowedMode()) return;

    ReleaseFullScreen();
  }
  virtual void RestoreVideo(void) {
    if (IsWindowedMode()) return;

    ShowWindow((HWND)game->GetMainWindow(), SW_SHOWNORMAL);
    AdjustWindow(GetModeWidth(), GetModeHeight(), GetModeBPP(),
                 IsWindowedMode());
  }
  virtual void AdjustForModeChange(void) {
    if (InEditMode()) return;

    // get previous size
    int nOldWidth = GetModeWidth();
    int nOldHeight = GetModeHeight();

    // Get the new mode info from the config record
    int nNewWidth = g_pMaterialSystemConfig->m_VideoMode.m_Width;
    int nNewHeight = g_pMaterialSystemConfig->m_VideoMode.m_Height;
    bool bWindowed = g_pMaterialSystemConfig->Windowed();

    // reset the window size
    CMatRenderContextPtr pRenderContext(materials);

    ResetCurrentModeForNewResolution(nNewWidth, nNewHeight, bWindowed);
    AdjustWindow(GetModeWidth(), GetModeHeight(), GetModeBPP(),
                 IsWindowedMode());
    MarkClientViewRectDirty();
    pRenderContext->Viewport(0, 0, GetModeWidth(), GetModeHeight());

    // fixup vgui
    vgui::surface()->OnScreenSizeChanged(nOldWidth, nOldHeight);
  }
  virtual void ReadScreenPixels(int x, int y, int w, int h, void *pBuffer,
                                ImageFormat format) {
    if (!g_LostVideoMemory) {
      bool bReadPixelsFromFrontBuffer =
          g_pMaterialSystemHardwareConfig->ReadPixelsFromFrontBuffer();
      if (bReadPixelsFromFrontBuffer) {
        Shader_SwapBuffers();
      }

      CMatRenderContextPtr pRenderContext(materials);

      Rect_t rect;
      rect.x = x;
      rect.y = y;
      rect.width = w;
      rect.height = h;

      pRenderContext->ReadPixelsAndStretch(
          &rect, &rect, (unsigned char *)pBuffer, format,
          w * ImageLoader::SizeInBytes(format));

      if (bReadPixelsFromFrontBuffer) {
        Shader_SwapBuffers();
      }
    } else {
      int nBytes = ImageLoader::GetMemRequired(w, h, 1, format, false);
      memset(pBuffer, 0, nBytes);
    }
  }

 private:
  virtual void ReleaseFullScreen(void) {
    if (IsWindowedMode()) return;

    // Hide the main window
    ChangeDisplaySettings(NULL, 0);
    ShowWindow((HWND)game->GetMainWindow(), SW_MINIMIZE);
  }
  virtual void ChangeDisplaySettingsToFullscreen(int nWidth, int nHeight,
                                                 int nBPP) {
    if (IsWindowedMode()) return;

    DEVMODE dm;
    memset(&dm, 0, sizeof(dm));

    dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = nWidth;
    dm.dmPelsHeight = nHeight;
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    dm.dmBitsPerPel = nBPP;

    // TODO(d.rattman): Fix direct reference of refresh rate from config record
    int freq = g_pMaterialSystemConfig->m_VideoMode.m_RefreshRate;
    if (freq >= 60) {
      dm.dmDisplayFrequency = freq;
      dm.dmFields |= DM_DISPLAYFREQUENCY;
    }

    ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
  }
};

void VideoMode_AdjustForModeChange() {
  ((CVideoMode_MaterialSystem *)videomode)->AdjustForModeChange();
}

IVideoMode *videomode = nullptr;

void VideoMode_Create() {
  videomode = new CVideoMode_MaterialSystem;
  Assert(videomode);
}

void VideoMode_Destroy() {
  if (videomode) {
    auto *pVideoMode_MS = static_cast<CVideoMode_MaterialSystem *>(videomode);
    delete pVideoMode_MS;
    videomode = nullptr;
  }
}
