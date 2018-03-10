// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "atl_headers.h"

#include <commctrl.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include "initguid.h"
#include "resource.h"
#include "steam_wmp_window.h"

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

#define ID_SKIP_FADE_TIMER 1
#define ID_DRAW_TIMER 2

const float FADE_TIME = 1.0f;
const int MAX_BLUR_STEPS = 100;

HWND g_hBlackFadingWindow = 0;
bool g_bFadeIn = true;
bool g_bFrameCreated = false;
SteamWMPWindow g_frame;
SteamWMPWindow *g_pFrame = nullptr;
HDC g_hdcCapture = 0;
HDC g_hdcBlend = 0;
HBITMAP g_hbmCapture = 0;
HBITMAP g_hbmBlend = 0;
HMONITOR g_hMonitor = 0;

int g_screenWidth = 0;
int g_screenHeight = 0;

std::wstring g_redirectTarget;
std::wstring g_URL;
bool g_bReportStats = false;
bool g_bUseLocalSteamServer = false;

double g_timeAtFadeStart = 0.0;
int g_nBlurSteps = 0;

HRESULT LogPlayerEvent(EventType e);
bool ShowFailureMessage(HRESULT hr);

CComPtr<IDirect3D9> g_pD3D = nullptr;
CComPtr<IDirect3DDevice9> g_pd3dDevice = nullptr;
CComPtr<IDirect3DVertexBuffer9> g_pDrawVB = nullptr;
CComPtr<IDirect3DTexture9> g_pImg = nullptr;
int g_nDrawStride = 0;
DWORD g_dwDrawFVF = 0;

DWORD g_dwUseVMROverlayOldValue = 0;
bool g_bUseVMROverlayValueExists = false;

LONG SetRegistryValue(const wchar_t *key_name, const wchar_t *value_name,
                      DWORD value, DWORD &old_Value, bool &was_value_set) {
  HKEY key = nullptr;
  LSTATUS return_code = RegCreateKeyExW(HKEY_CURRENT_USER, key_name, 0, nullptr,
                                        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                                        nullptr, &key, nullptr);
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to open registry key: "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
    return return_code;
  }

  DWORD key_type = 0, value_size = sizeof(old_Value);

  // amusingly enough, if pValueName doesn't exist, RegQueryValueEx returns
  // ERROR_FILE_NOT_FOUND
  return_code = RegQueryValueExW(key, value_name, nullptr, &key_type,
                                 (LPBYTE)&old_Value, &value_size);
  was_value_set = return_code == ERROR_SUCCESS;

  return_code = RegSetValueExW(key, value_name, 0, REG_DWORD,
                               (const BYTE *)&value, sizeof(value));
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to write registry value "));
    OutputDebugString(value_name);
    OutputDebugString(_T(" in key "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
  }

  return_code = RegCloseKey(key);
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to close registry key "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
  }

  return return_code;
}

LONG RestoreRegistryValue(const wchar_t *key_name, const wchar_t *value_name,
                          DWORD old_value, bool has_old_value) {
  HKEY key = nullptr;
  LONG return_code = RegCreateKeyExW(HKEY_CURRENT_USER, key_name, 0, nullptr,
                                     REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                                     nullptr, &key, nullptr);
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to open registry key: "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
    return return_code;
  }

  if (has_old_value) {
    return_code = RegSetValueExW(key, value_name, 0, REG_DWORD,
                                 (const BYTE *)&old_value, sizeof(old_value));
    if (return_code != ERROR_SUCCESS) {
      OutputDebugString(_T("Can't restore registry key's "));
      OutputDebugString(key_name);
      OutputDebugString(_T(" value "));
      OutputDebugString(value_name);
    }
  } else {
    return_code = RegDeleteValueW(key, value_name);
    if (return_code != ERROR_SUCCESS) {
      OutputDebugString(_T("Can't delete registry key's "));
      OutputDebugString(key_name);
      OutputDebugString(_T(" value "));
      OutputDebugString(value_name);
    }
  }

  return_code = RegCloseKey(key);
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to close registry key "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
  }

  return return_code;
}

bool GetRegistryString(const wchar_t *key_name, const wchar_t *value_name,
                       const wchar_t *value_string, DWORD value_length) {
  HKEY key = nullptr;
  LONG return_code = RegCreateKeyEx(HKEY_CURRENT_USER, key_name, 0, nullptr,
                                    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                                    nullptr, &key, nullptr);
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to open registry key: "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
    return false;
  }

  DWORD key_type = 0;
  return_code = RegQueryValueEx(key, value_name, nullptr, &key_type,
                                (LPBYTE)value_string, (DWORD *)&value_length);
  const bool was_key_read = return_code == ERROR_SUCCESS && key_type == REG_SZ;

  return_code = RegCloseKey(key);
  if (return_code != ERROR_SUCCESS) {
    OutputDebugString(_T("unable to close registry key "));
    OutputDebugString(key_name);
    OutputDebugString(_T("\n"));
  }

  if (!was_key_read) {
    OutputDebugString(_T("unable to read registry string: "));
    OutputDebugString(value_name);
    OutputDebugString(_T("\n"));
    return false;
  }

  return true;
}

struct EventData {
  EventData(int t, double pos, EventType e)
      : time{t}, position{pos}, event{e} {}
  int time;         // real time
  double position;  // movie position
  EventType event;  // event type
};

const char *GetEventName(EventType event) {
  switch (event) {
    case ET_APPLAUNCH:
      return "al";
    case ET_APPEXIT:
      return "ae";
    case ET_CLOSE:
      return "cl";
    case ET_FADEOUT:
      return "fo";

    case ET_MEDIABEGIN:
      return "mb";
    case ET_MEDIAEND:
      return "me";

    case ET_JUMPHOME:
      return "jh";
    case ET_JUMPEND:
      return "je";

    case ET_PLAY:
      return "pl";
    case ET_PAUSE:
      return "ps";
    case ET_STOP:
      return "st";
    case ET_SCRUBFROM:
      return "jf";
    case ET_SCRUBTO:
      return "jt";
    case ET_STEPFWD:
      return "sf";
    case ET_STEPBCK:
      return "sb";
    case ET_JUMPFWD:
      return "jf";
    case ET_JUMPBCK:
      return "jb";
    case ET_REPEAT:
      return "rp";

    case ET_MAXIMIZE:
      return "mx";
    case ET_MINIMIZE:
      return "mn";
    case ET_RESTORE:
      return "rs";

    default:
      return "<unknown>";
  }
}

std::vector<EventData> g_events;

void LogPlayerEvent(EventType e, double pos) {
  if (!g_bReportStats) return;

  static int s_firstTick = GetTickCount();
  int time = GetTickCount() - s_firstTick;

  wchar_t msg[256];
  wsprintfW(msg, _T("event %s at time %d and pos %.2f\n"), GetEventName(e),
            time, 1000 * pos);
  OutputDebugString(msg);

  bool bDropEvent = false;

  size_t nEvents = g_events.size();
  if ((e == ET_STEPFWD || e == ET_STEPBCK) && nEvents >= 2) {
    const EventData &e1 = g_events[nEvents - 1];
    const EventData &e2 = g_events[nEvents - 2];
    if ((e1.event == e || e1.event == ET_REPEAT) && e2.event == e) {
      // only store starting and ending stepfwd or stepbck events, since there
      // can be so many also keep events that are more than a second apart
      if (e1.event == ET_REPEAT) {
        // keep dropping events while e1 isn't before a gap
        bDropEvent = time - e1.time < 1000;
      } else {
        // e2 was kept last time, so keep e1 if e2 was kept because it was
        // before a gap
        bDropEvent = e1.time - e2.time < 1000;
      }
    }
  }

  if (bDropEvent) {
    g_events[nEvents - 1] = EventData(time, pos, ET_REPEAT);
  } else {
    g_events.push_back(EventData(time, pos, e));
  }
}

char C2M_UPLOADDATA = 'q';
char C2M_UPLOADDATA_PROTOCOL_VERSION = 1;
char C2M_UPLOADDATA_DATA_VERSION = 1;

inline void WriteHexDigit(std::ostream &os, byte src) {
  os.put((src <= 9) ? src + '0' : src - 10 + 'A');
}

inline void WriteByte(std::ostream &os, byte src) {
  WriteHexDigit(os, src >> 4);
  WriteHexDigit(os, src & 0xf);
}

inline void WriteShort(std::ostream &os, unsigned short src) {
  WriteByte(os, (byte)((src >> 8) & 0xff));
  WriteByte(os, (byte)(src & 0xff));
}

inline void WriteInt24(std::ostream &os, int src) {
  WriteByte(os, (byte)((src >> 16) & 0xff));
  WriteByte(os, (byte)((src >> 8) & 0xff));
  WriteByte(os, (byte)(src & 0xff));
}

inline void WriteInt(std::ostream &os, int src) {
  WriteByte(os, (byte)((src >> 24) & 0xff));
  WriteByte(os, (byte)((src >> 16) & 0xff));
  WriteByte(os, (byte)((src >> 8) & 0xff));
  WriteByte(os, (byte)(src & 0xff));
}

inline void WriteFloat(std::ostream &os, float src) {
  WriteInt(os, *(int *)&src);
}

void WriteUUID(std::ostream &os, const UUID &uuid) {
  WriteInt(os, uuid.Data1);
  WriteShort(os, uuid.Data2);
  WriteShort(os, uuid.Data3);
  for (int i = 0; i < 8; ++i) {
    WriteByte(os, uuid.Data4[i]);
  }
}

bool QueryOrGenerateUserID(UUID &userId) {
  HKEY hKey = 0;
  LONG rval = RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Valve\\Steam"), 0,
                             nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                             nullptr, &hKey, nullptr);
  if (rval != ERROR_SUCCESS) {
    UuidCreate(&userId);
    return false;
  }

  DWORD dwType = 0;
  unsigned short idstr[40];
  DWORD dwSize = sizeof(idstr);

  rval = RegQueryValueEx(hKey, _T("smpid"), nullptr, &dwType, (LPBYTE)idstr,
                         &dwSize);
  if (rval != ERROR_SUCCESS || dwType != REG_SZ) {
    UuidCreate(&userId);

    unsigned short *outstring = nullptr;
    UuidToString(&userId, &outstring);
    if (outstring == nullptr || *outstring == '\0') {
      RegCloseKey(hKey);
      return false;
    }

    rval = RegSetValueEx(hKey, _T("smpid"), 0, REG_SZ, (CONST BYTE *)outstring,
                         sizeof(idstr));

    RpcStringFree(&outstring);
    RegCloseKey(hKey);

    return rval == ERROR_SUCCESS;
  }

  if (RPC_S_OK != UuidFromString(idstr, &userId)) return false;

  RegCloseKey(hKey);
  return true;
}

void PrintStats(const wchar_t *pStatsFilename) {
  std::wofstream os(pStatsFilename, std::ios_base::out | std::ios_base::binary);

  // user id
  UUID userId;
  QueryOrGenerateUserID(userId);
  unsigned short *userIdStr;
  UuidToString(&userId, &userIdStr);
  os << userIdStr << _T("\n");
  RpcStringFree(&userIdStr);

  // filename
  size_t nOffset = g_URL.find_last_of(_T("/\\"));
  if (nOffset == g_URL.npos) nOffset = 0;

  std::wstring filename = g_URL.substr(nOffset + 1);
  os << filename << _T('\n');

  // number of events
  size_t nEvents = g_events.size();
  os << nEvents << _T("\n");

  // event data (tab-delimited)
  for (int i = 0; i < nEvents; ++i) {
    os << GetEventName(g_events[i].event) << _T("\t");
    os << g_events[i].time << L"\t";
    os << int(1000 * g_events[i].position) << _T("\n");
  }
}

void UploadStats() {
  wchar_t pathname[256];
  if (!GetRegistryString(_T("Software\\Valve\\Steam"), _T("SteamExe"), pathname,
                         sizeof(pathname)))
    return;

  wchar_t *pExeName = wcsrchr(pathname, L'/');
  if (!pExeName) return;

  *pExeName = _T('\0');  // truncate exe filename to just pathname

  wchar_t filename[256];
  wsprintf(filename, _T("%s/smpstats.txt"), pathname);

  PrintStats(filename);

  ShellExecute(nullptr, _T("open"), _T("steam://smp/smpstats.txt"), nullptr,
               nullptr, SW_SHOWNORMAL);
}

void RestoreRegistry() {
  static bool s_bDone = false;
  if (s_bDone) return;

  s_bDone = true;
  RestoreRegistryValue(
      _T("Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings"),
      _T("UseVMROverlay"), g_dwUseVMROverlayOldValue,
      g_bUseVMROverlayValueExists);
}

void CleanupD3D() {
  g_pDrawVB = nullptr;
  g_pImg = nullptr;
  g_pd3dDevice = nullptr;
  g_pD3D = nullptr;
}

struct VertexShaderInfo {
  IDirect3DVertexShader9 *Shader;
  IDirect3DVertexDeclaration9 *Declaration;
};

void InitTextureStageState(int nStage, DWORD dwColorOp, DWORD dwColorArg1,
                           DWORD dwColorArg2,
                           DWORD dwColorArg0 = D3DTA_CURRENT) {
  g_pd3dDevice->SetTextureStageState(nStage, D3DTSS_COLOROP, dwColorOp);
  g_pd3dDevice->SetTextureStageState(nStage, D3DTSS_COLORARG1, dwColorArg1);
  g_pd3dDevice->SetTextureStageState(nStage, D3DTSS_COLORARG2, dwColorArg2);
  g_pd3dDevice->SetTextureStageState(nStage, D3DTSS_COLORARG0, dwColorArg0);
  g_pd3dDevice->SetTextureStageState(nStage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
  g_pd3dDevice->SetSamplerState(nStage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  g_pd3dDevice->SetSamplerState(nStage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  g_pd3dDevice->SetSamplerState(nStage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  g_pd3dDevice->SetSamplerState(nStage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
}

HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DDevice9 *pD3D9Device,
                                    IDirect3DSurface9 *pSourceSurface,
                                    const RECT *pSourceRectsArray, UINT cRects,
                                    IDirect3DSurface9 *pDestinationSurface,
                                    const POINT *pDestPointsArray) {
  if (pSourceSurface == nullptr || pDestinationSurface == nullptr ||
      pSourceSurface == pDestinationSurface) {
    return D3DERR_INVALIDCALL;
  }

  D3DSURFACE_DESC SourceDesc, DestinationDesc;
  pSourceSurface->GetDesc(&SourceDesc);
  pDestinationSurface->GetDesc(&DestinationDesc);

  if (SourceDesc.Format != DestinationDesc.Format) {
    return D3DERR_INVALIDCALL;
  }

  HRESULT hr = D3DERR_INVALIDCALL;

  if (cRects == 0) {
    cRects = 1;
  }

  for (UINT i = 0; i < cRects; i++) {
    RECT SourceRect, DestinationRect;

    if (pSourceRectsArray != nullptr) {
      SourceRect = pSourceRectsArray[i];
    } else {
      SourceRect.left = 0;
      SourceRect.right = SourceDesc.Width;
      SourceRect.top = 0;
      SourceRect.bottom = SourceDesc.Height;
    }

    if (pDestPointsArray != nullptr) {
      DestinationRect.left = pDestPointsArray[i].x;
      DestinationRect.right =
          DestinationRect.left + (SourceRect.right - SourceRect.left);
      DestinationRect.top = pDestPointsArray[i].y;
      DestinationRect.bottom =
          DestinationRect.top + (SourceRect.bottom - SourceRect.top);
    } else {
      DestinationRect = SourceRect;
    }

    if (SourceDesc.Pool == D3DPOOL_MANAGED ||
        DestinationDesc.Pool != D3DPOOL_DEFAULT) {
      hr = D3DERR_INVALIDCALL;
    } else if (SourceDesc.Pool == D3DPOOL_DEFAULT) {
      hr = pD3D9Device->StretchRect(pSourceSurface, &SourceRect,
                                    pDestinationSurface, &DestinationRect,
                                    D3DTEXF_NONE);
    } else if (SourceDesc.Pool == D3DPOOL_SYSTEMMEM) {
      const POINT pt = {DestinationRect.left, DestinationRect.top};

      hr = pD3D9Device->UpdateSurface(pSourceSurface, &SourceRect,
                                      pDestinationSurface, &pt);
    }

    if (FAILED(hr)) {
      break;
    }
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DDevice9 *pd3dDevice,
                                          DWORD Handle) {
  HRESULT hr;

  if ((Handle & 0x80000000) == 0) {
    pd3dDevice->SetVertexShader(nullptr);
    hr = pd3dDevice->SetFVF(Handle);
  } else {
    const DWORD handleMagic = Handle << 1;
    VertexShaderInfo *const ShaderInfo = reinterpret_cast<VertexShaderInfo *>(
        static_cast<UINT_PTR>(handleMagic));

    hr = pd3dDevice->SetVertexShader(ShaderInfo->Shader);
    pd3dDevice->SetVertexDeclaration(ShaderInfo->Declaration);
  }

  return hr;
}

bool InitD3D(HWND hWnd, bool blur) {
  g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  if (!g_pD3D) {
    OutputDebugString(_T("Direct3DCreate9 FAILED!\n"));
    CleanupD3D();
    return false;
  }

  D3DDISPLAYMODE d3ddm{0};
  bool bFound = false;
  int nAdapters = g_pD3D->GetAdapterCount();
  int nAdapterIndex = 0;
  for (; nAdapterIndex < nAdapters; ++nAdapterIndex) {
    if (g_pD3D->GetAdapterMonitor(nAdapterIndex) == g_hMonitor) {
      if (FAILED(g_pD3D->GetAdapterDisplayMode(nAdapterIndex, &d3ddm))) {
        OutputDebugString(_T("GetAdapterDisplayMode FAILED!\n"));
        CleanupD3D();
        return false;
      }

      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      GetMonitorInfoW(g_hMonitor, &mi);
      bFound = true;
      break;
    }
  }
  if (!bFound) {
    OutputDebugString(
        _T("Starting monitor not found when creating D3D device!\n"));
    CleanupD3D();
    return false;
  }

  D3DPRESENT_PARAMETERS d3dpp;
  ZeroMemory(&d3dpp, sizeof(d3dpp));
  d3dpp.BackBufferWidth = g_screenWidth;
  d3dpp.BackBufferHeight = g_screenHeight;
  d3dpp.BackBufferFormat = d3ddm.Format;
  d3dpp.BackBufferCount = 1;
  d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
  d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  d3dpp.hDeviceWindow = nullptr;
  d3dpp.Windowed = FALSE;
  d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
  d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

  if (FAILED(g_pD3D->CreateDevice(nAdapterIndex, D3DDEVTYPE_HAL, hWnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp,
                                  &g_pd3dDevice))) {
    OutputDebugString(_T("CreateDevice FAILED!\n"));
    CleanupD3D();
    return false;
  }

  // create and fill vertex buffer(s)
  float du = 0.5f / g_screenWidth;
  float dv = 0.5f / g_screenHeight;
  float u0 = du;
  float u1 = 1.0f + du;
  float v0 = dv;
  float v1 = 1.0f + dv;
  float drawverts[] = {
      // x, y, z, u, v
      -1, -1, 0, u0, v0, -1, 1, 0, u0, v1, 1, -1, 0, u1, v0, 1, 1, 0, u1, v1,
  };
  g_dwDrawFVF = D3DFVF_XYZ | D3DFVF_TEX1;
  g_nDrawStride = sizeof(drawverts) / 4;

  if (FAILED(g_pd3dDevice->CreateVertexBuffer(
          sizeof(drawverts), D3DUSAGE_WRITEONLY, g_dwDrawFVF, D3DPOOL_MANAGED,
          &g_pDrawVB, nullptr))) {
    OutputDebugString(_T("CreateVertexBuffer( g_pDrawVB ) FAILED!\n"));
    CleanupD3D();
    return false;
  }

  BYTE *pDrawVBMem;
  if (FAILED(g_pDrawVB->Lock(0, sizeof(drawverts), (void **)&pDrawVBMem, 0))) {
    OutputDebugString(_T("g_pDrawVB->Lock FAILED!\n"));
    CleanupD3D();
    return false;
  }
  memcpy(pDrawVBMem, drawverts, sizeof(drawverts));
  g_pDrawVB->Unlock();

  g_pd3dDevice->SetStreamSource(0, g_pDrawVB, 0, g_nDrawStride);
  SetVertexShader(g_pd3dDevice, g_dwDrawFVF);

  // create and fill texture
  if (FAILED(g_pd3dDevice->CreateTexture(g_screenWidth, g_screenHeight, 1, 0,
                                         D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                         &g_pImg, nullptr))) {
    OutputDebugString(_T("CreateTexture( g_pImg ) FAILED!\n"));
    CleanupD3D();
    return false;
  }

  D3DLOCKED_RECT lockedRect;
  if (FAILED(g_pImg->LockRect(0, &lockedRect, nullptr, 0))) {
    OutputDebugString(_T("g_pImg->LockRect FAILED!\n"));
    CleanupD3D();
    return false;
  }

  BITMAPINFO bitmapInfo;
  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = g_screenWidth;
  bitmapInfo.bmiHeader.biHeight = g_screenHeight;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;
  bitmapInfo.bmiHeader.biSizeImage = 0;
  bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
  bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
  bitmapInfo.bmiHeader.biClrUsed = 0;
  bitmapInfo.bmiHeader.biClrImportant = 0;

  if (GetDIBits(g_hdcCapture, g_hbmCapture, 0, g_screenHeight, lockedRect.pBits,
                &bitmapInfo, DIB_RGB_COLORS) != g_screenHeight) {
    OutputDebugString(_T("GetDIBits FAILED to get the full image!\n"));
  }

  g_pImg->UnlockRect(0);
  g_pd3dDevice->SetTexture(0, g_pImg);

  InitTextureStageState(0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR);

  return true;
}

HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DDevice9 *pD3D9Device,
                                          IDirect3DSurface9 *pRenderTarget,
                                          IDirect3DSurface9 *pNewZStencil) {
  HRESULT hr;

  if (pRenderTarget != nullptr) {
    hr = pD3D9Device->SetRenderTarget(0, pRenderTarget);

    if (FAILED(hr)) {
      return hr;
    }
  }

  if (pNewZStencil != nullptr) {
    hr = pD3D9Device->SetDepthStencilSurface(pNewZStencil);

    if (FAILED(hr)) {
      return hr;
    }
  } else {
    pD3D9Device->SetDepthStencilSurface(nullptr);
  }

  return D3D_OK;
}

void DrawD3DFade(BYTE fade, bool blur) {
  if (g_pd3dDevice) {
    g_pd3dDevice->BeginScene();

    DWORD factor = fade | (fade << 8) | (fade << 16) | (fade << 24);
    g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, factor);
    g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    g_pd3dDevice->EndScene();
    g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
  }
}

int g_nTimingIndex = 0;
double g_timings[65536];

LRESULT CALLBACK SteamMediaPlayerWndProc(HWND hWnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam) {
  switch (msg) {
    case WM_CREATE: {
      g_timeAtFadeStart = 0.0;
      g_nBlurSteps = 0;

      g_hBlackFadingWindow = hWnd;

      InitD3D(hWnd, true);

      if (!g_bFadeIn) {
        g_pFrame->ShowWindow(SW_HIDE);
        g_pFrame->PostMessage(WM_DESTROY, 0, 0);
      }

      InvalidateRect(hWnd, nullptr, TRUE);

      SetTimer(hWnd, ID_SKIP_FADE_TIMER, 1000,
               nullptr);  // if the fade doesn't start in 1 second, then just
                          // jump to the video
      SetTimer(hWnd, ID_DRAW_TIMER, 10, nullptr);  // draw timer
    } break;

    case WM_TIMER:
      if (wparam == ID_DRAW_TIMER) {
        static LARGE_INTEGER s_nPerformanceFrequency;
        if (g_timeAtFadeStart == 0.0) {
          LARGE_INTEGER nTimeAtFadeStart;
          QueryPerformanceCounter(&nTimeAtFadeStart);
          QueryPerformanceFrequency(&s_nPerformanceFrequency);
          g_timeAtFadeStart = nTimeAtFadeStart.QuadPart /
                              double(s_nPerformanceFrequency.QuadPart);

          KillTimer(hWnd, ID_SKIP_FADE_TIMER);
          // restart skip fade timer and give it an extra 100ms
          // to allow the fade to draw fully black once
          SetTimer(hWnd, ID_SKIP_FADE_TIMER, 100 + UINT(FADE_TIME * 1000),
                   nullptr);
        }

        LARGE_INTEGER time;
        QueryPerformanceCounter(&time);
        g_timings[g_nTimingIndex++] =
            time.QuadPart / double(s_nPerformanceFrequency.QuadPart);
        float dt =
            (float)(time.QuadPart / double(s_nPerformanceFrequency.QuadPart) -
                    g_timeAtFadeStart);

        bool bFadeFinished = dt >= FADE_TIME;
        float fraction = bFadeFinished ? 1.0f : dt / FADE_TIME;

        bool blur =
            g_bFadeIn && (int(fraction * MAX_BLUR_STEPS) > g_nBlurSteps);
        if (blur) {
          ++g_nBlurSteps;
        }

        BYTE fade = BYTE(fraction * 255.999f);
        if (g_bFadeIn) fade = 255 - fade;

        DrawD3DFade(fade, blur);

        if (!bFadeFinished) break;
      }

      KillTimer(hWnd, ID_SKIP_FADE_TIMER);
      KillTimer(hWnd, ID_DRAW_TIMER);

      if (!g_bFadeIn) {
        ShowWindow(hWnd, SW_HIDE);
        PostMessageW(hWnd, WM_CLOSE, 0, 0);
        return 1;
      }

      if (!g_bFrameCreated) {
        g_bFrameCreated = true;

        CleanupD3D();

        g_pFrame = &g_frame;
        g_pFrame->GetWndClassInfo().m_wc.hIcon =
            LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_ICON));

        RECT rcPos = {CW_USEDEFAULT, 0, 0, 0};

        g_pFrame->Create(GetDesktopWindow(), rcPos, _T("Steam Media Player"),
                         WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, (UINT)0);
        g_pFrame->ShowWindow(SW_SHOW);
      }

      // close WMP window once we paint the fullscreen fade window
      if (!g_bFadeIn) g_pFrame->ShowWindow(SW_HIDE);

      return 1;

    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) ::DestroyWindow(hWnd);
      break;

    case WM_DESTROY:
      g_hBlackFadingWindow = nullptr;

      CleanupD3D();

      if (g_bFrameCreated) {
        g_bFrameCreated = false;

        g_pFrame->DestroyWindow();
        g_pFrame = nullptr;
      }

      ::PostQuitMessage(0);
      break;
  }

  return DefWindowProcW(hWnd, msg, wparam, lparam);
}

bool ShowFadeWindow(bool bShow) {
  if (bShow) {
    g_timeAtFadeStart = 0.0;
    g_bFadeIn = false;

    SetTimer(g_hBlackFadingWindow, ID_DRAW_TIMER, 10, nullptr);

    if (g_pFrame) g_pFrame->ShowWindow(SW_HIDE);

    InitD3D(g_hBlackFadingWindow, false);
    InvalidateRect(g_hBlackFadingWindow, nullptr, TRUE);
  } else {
    SetWindowPos(g_hBlackFadingWindow, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW |
                     SWP_NOACTIVATE | SWP_DEFERERASE);
  }

  return true;
}

std::tuple<HWND, HRESULT> CreateFullscreenWindow(HINSTANCE instance,
                                                 HMONITOR monitor,
                                                 bool bFadeIn) {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = SteamMediaPlayerWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = instance;
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hCursor = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = _T("SteamMediaPlayer_WndClass001");

  if (!RegisterClassExW(&wc)) {
    return {nullptr, HRESULT_FROM_WIN32(GetLastError())};
  }

  g_bFadeIn = bFadeIn;

  MONITORINFO mi{sizeof(mi)};
  if (!GetMonitorInfoW(monitor, &mi)) {
    GetClientRect(GetDesktopWindow(), &mi.rcMonitor);
  }

  HWND hwnd =
      CreateWindowExW(0L, _T("SteamMediaPlayer_WndClass001"),
                      _T("Steam Media Player"), WS_POPUP, mi.rcMonitor.left,
                      mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                      mi.rcMonitor.bottom - mi.rcMonitor.top, nullptr, nullptr,
                      instance, nullptr);
  if (!hwnd) {
    return {nullptr, HRESULT_FROM_WIN32(GetLastError())};
  }

  while (ShowCursor(FALSE) >= 0)
    ;

  return {hwnd, S_OK};
}

HRESULT CreateDesktopBitmaps(HMONITOR monitor) {
  MONITORINFOEX mi;
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) return E_FAIL;

  g_screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  g_screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  HDC hdcScreen = CreateDCW(mi.szDevice, mi.szDevice, nullptr, nullptr);
  if (!hdcScreen) return E_FAIL;

  g_hdcCapture = CreateCompatibleDC(hdcScreen);
  g_hdcBlend = CreateCompatibleDC(hdcScreen);

  if (!g_hdcCapture || !g_hdcBlend) {
    DeleteDC(hdcScreen);
    return false;
  }

  if ((GetDeviceCaps(hdcScreen, SHADEBLENDCAPS) & SB_CONST_ALPHA) == 0) {
    OutputDebugString(_T("display doesn't support AlphaBlend!\n"));
  }

  if ((GetDeviceCaps(hdcScreen, RASTERCAPS) & RC_BITBLT) == 0) {
    OutputDebugString(_T("display doesn't support BitBlt!\n"));
  }

  if (GetDeviceCaps(hdcScreen, BITSPIXEL) < 32) {
    OutputDebugString(_T("display doesn't support 32bpp!\n"));
  }

  if (g_screenWidth != GetDeviceCaps(hdcScreen, HORZRES) ||
      g_screenHeight != GetDeviceCaps(hdcScreen, VERTRES)) {
    OutputDebugString(_T("Screen DC size differs from monitor size!\n"));
  }

  g_hbmCapture =
      CreateCompatibleBitmap(hdcScreen, g_screenWidth, g_screenHeight);
  g_hbmBlend = CreateCompatibleBitmap(hdcScreen, g_screenWidth, g_screenHeight);
  if (!g_hbmCapture || !g_hbmBlend) {
    DeleteDC(hdcScreen);
    return E_FAIL;
  }

  HGDIOBJ oldCaptureObject = SelectObject(g_hdcCapture, g_hbmCapture);
  HGDIOBJ oldBlendObject = SelectObject(g_hdcBlend, g_hbmBlend);

  const bool is_bitblt_ok{!!BitBlt(g_hdcCapture, 0, 0, g_screenWidth,
                                   g_screenHeight, hdcScreen, 0, 0, SRCCOPY)};

  DeleteDC(hdcScreen);

  const bool is_capture_restored{HGDI_ERROR !=
                                 SelectObject(g_hdcCapture, oldCaptureObject)};
  const bool is_blend_restored{HGDI_ERROR !=
                               SelectObject(g_hdcBlend, oldBlendObject)};

  if (!is_bitblt_ok) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  return is_capture_restored && is_blend_restored ? S_OK : E_FAIL;
}

void PrintLastError(const wchar_t *user_message,
                    const DWORD error_code = GetLastError()) {
#ifndef NDEBUG
  wchar_t *buffer;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (wchar_t *)&buffer, 0, nullptr);

  wchar_t error_message[512];
  _snwprintf_s(error_message, ARRAYSIZE(error_message) - 1, _T("%s (%d) %s"),
               user_message, error_code, buffer);
  LocalFree(buffer);

  OutputDebugStringW(error_message);
#endif
}

void ParseCommandLine(const wchar_t *command_line) {
  std::vector<std::wstring> params;
  params.push_back(_T(""));

  bool is_quoted = false;
  for (const wchar_t *cp = command_line; *cp; ++cp) {
    if (*cp == _T('\"')) {
      is_quoted = !is_quoted;
    } else if (iswspace(*cp) && !is_quoted) {
      if (!params.back().empty()) {
        params.push_back(_T(""));
      }
    } else {
      params.back().push_back(*cp);
    }
  }

  if (params.back().empty()) {
    params.pop_back();
  }

  const size_t params_size = params.size();

  for (size_t i = 0; i < params_size; ++i) {
    if (params[i][0] == '-' || params[i][0] == '/') {
      const wchar_t *key = params[i].c_str() + 1;

      if (wcscmp(key, _T("reportstats")) == 0) {
        g_bReportStats = true;
      } else if (wcscmp(key, _T("localsteamserver")) == 0) {
        g_bUseLocalSteamServer = true;
      } else if (wcscmp(key, _T("redirect")) == 0) {
        ++i;
        g_redirectTarget = params[i];
      }
    } else {
      g_URL = params[i];
    }
  }
}

std::tuple<int, HRESULT> DispatchMessages() {
  MSG message;
  BOOL is_message_ok;

  while ((is_message_ok = GetMessageW(&message, nullptr, 0, 0)) != 0) {
    if (is_message_ok != -1) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    } else {
      return {-1, HRESULT_FROM_WIN32(GetLastError())};
    }
  }

  return {static_cast<int>(message.wParam), S_OK};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t *cmd, int) {
  if (cmd == nullptr || *cmd == L'\0') return 0;

  cmd = GetCommandLineW();
  ParseCommandLine(cmd);

  SetRegistryValue(
      _T("Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings"),
      _T("UseVMROverlay"), 0, g_dwUseVMROverlayOldValue,
      g_bUseVMROverlayValueExists);
  atexit(RestoreRegistry);

  LogPlayerEvent(ET_APPLAUNCH, 0.0);

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                           COINIT_DISABLE_OLE1DDE |
                                           COINIT_SPEED_OVER_MEMORY);
  CComModule com_module;

  if (SUCCEEDED(hr)) {
    hr = com_module.Init(ObjectMap, instance, &LIBID_ATLLib);
  }

  POINT cursor_position{0};
  if (SUCCEEDED(hr)) {
    hr = GetCursorPos(&cursor_position) ? S_OK
                                        : HRESULT_FROM_WIN32(GetLastError());
  }

  if (SUCCEEDED(hr)) {
    g_hMonitor = MonitorFromPoint(cursor_position, MONITOR_DEFAULTTONEAREST);
  }

  if (SUCCEEDED(hr)) {
    hr = CreateDesktopBitmaps(g_hMonitor);
  }

  if (SUCCEEDED(hr)) {
    ShowCursor(FALSE);

    std::tie(g_hBlackFadingWindow, hr) =
        CreateFullscreenWindow(instance, g_hMonitor, true);
  }

  int return_code = 0;
  if (SUCCEEDED(hr)) {
    std::tie(return_code, hr) = DispatchMessages();
  }

  LogPlayerEvent(ET_APPEXIT);

  if (return_code == 0 && SUCCEEDED(hr) && g_bReportStats) UploadStats();

  if (!g_redirectTarget.empty()) {
    ShellExecuteW(nullptr, _T("open"), g_redirectTarget.c_str(), nullptr,
                  nullptr, SW_SHOWNORMAL);
  }

  CoUninitialize();
  com_module.Term();

  RestoreRegistry();

  return SUCCEEDED(hr) ? return_code : static_cast<int>(hr);
}
