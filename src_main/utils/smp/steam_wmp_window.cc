// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "steam_wmp_window.h"

#include "atl_headers.h"

#include <comutil.h>
#include <cassert>
#include <cstdio>
#include <string>

extern HWND g_hBlackFadingWindow;
extern SteamWMPWindow* g_pFrame;
extern bool g_bFrameCreated;
extern double g_timeAtFadeStart;
extern bool g_bFadeIn;
extern bool g_bFadeWindowTriggered;
extern std::wstring g_URL;

bool ShowFadeWindow(bool bShow);
void LogPlayerEvent(EventType e, double pos);

CComPtr<IWMPPlayer> g_spWMPPlayer;
bool g_bWantToBeFullscreen = false;
bool g_bPlayOnRestore = false;
int g_desiredVideoScaleMode = ID_STRETCH_TO_FIT;

bool ShowFailureMessage(HRESULT hr) {
  const bool is_succeeded = SUCCEEDED(hr);
  if (!is_succeeded) {
    wchar_t error_message[64];

    wsprintfW(error_message, _T("Error code = %08X"), hr);
    MessageBoxW(nullptr, error_message, _T("Steam Media Player - Error"),
                MB_OK | MB_ICONERROR);
  }

  return !is_succeeded;
}

HRESULT LogPlayerEvent(EventType e) {
  CComPtr<IWMPControls> wmp_controls;
  HRESULT hr =
      g_spWMPPlayer ? g_spWMPPlayer->get_controls(&wmp_controls) : S_OK;

  double position = 0.0;
  if (SUCCEEDED(hr) && wmp_controls) {
    hr = wmp_controls->get_currentPosition(&position);
  }

  LogPlayerEvent(e, position);

  return hr;
}

bool IsStretchedToFit() {
  if (!g_spWMPPlayer) return false;

  CComPtr<IWMPPlayer2> wmp_player2;
  HRESULT hr = g_spWMPPlayer.QueryInterface(&wmp_player2);

  VARIANT_BOOL is_stretched_to_fit = VARIANT_FALSE;
  if (SUCCEEDED(hr) && wmp_player2) {
    hr = wmp_player2->get_stretchToFit(&is_stretched_to_fit);
  }

  return SUCCEEDED(hr) && is_stretched_to_fit == VARIANT_TRUE;
}

bool SetVideoScaleMode(int videoScaleMode) {
  g_desiredVideoScaleMode = videoScaleMode;

  if (!g_spWMPPlayer) return false;

  long width = 0, height = 0;

  CComPtr<IWMPMedia> wmp_media;
  HRESULT hr = g_spWMPPlayer->get_currentMedia(&wmp_media);

  if (SUCCEEDED(hr) && wmp_media) {
    hr = wmp_media->get_imageSourceWidth(&width);
  }

  if (SUCCEEDED(hr) && wmp_media) {
    hr = wmp_media->get_imageSourceHeight(&height);
  }

  VARIANT_BOOL need_stretch_to_fit = VARIANT_FALSE;
  switch (videoScaleMode) {
    case ID_HALF_SIZE:
      // TODO - set video player size
      break;
    case ID_FULL_SIZE:
      // TODO - set video player size
      break;
    case ID_DOUBLE_SIZE:
      // TODO - set video player size
      break;

    case ID_STRETCH_TO_FIT:
      need_stretch_to_fit = VARIANT_TRUE;
      break;

    default:
      return false;
  }

  CComPtr<IWMPPlayer2> wmp_player2;
  hr = g_spWMPPlayer.QueryInterface(&wmp_player2);

  const bool is_stretch_to_fit = need_stretch_to_fit == VARIANT_TRUE;

  if (SUCCEEDED(hr) && wmp_player2) {
    if (is_stretch_to_fit == IsStretchedToFit()) return true;

    hr = wmp_player2->put_stretchToFit(need_stretch_to_fit);
  }

  return SUCCEEDED(hr) && is_stretch_to_fit == IsStretchedToFit();
}

bool IsFullScreen() {
  if (!g_spWMPPlayer) return false;

  VARIANT_BOOL is_fullscreen = VARIANT_TRUE;
  return SUCCEEDED(g_spWMPPlayer->get_fullScreen(&is_fullscreen)) &&
         is_fullscreen == VARIANT_TRUE;
}

bool SetFullScreen(bool should_go_fullscreen) {
  g_bWantToBeFullscreen = should_go_fullscreen;

  if (!g_spWMPPlayer) return false;

  LogPlayerEvent(should_go_fullscreen ? ET_MAXIMIZE : ET_RESTORE);

  bool is_fullscreen = IsFullScreen();

  CComPtr<IWMPPlayer2> wmp_player2;
  HRESULT hr = g_spWMPPlayer.QueryInterface(&wmp_player2);
  VARIANT_BOOL vbStretch = VARIANT_TRUE;

  if (SUCCEEDED(hr) && wmp_player2) {
    hr = wmp_player2->get_stretchToFit(&vbStretch);
  } else {
    // The MS documentation claims this shouldn't happen, but it does...
    assert(wmp_player2 == nullptr);
    wmp_player2 = nullptr;
  }

  bool bStretch = SUCCEEDED(hr) && vbStretch == VARIANT_TRUE;
  if (bStretch != g_bWantToBeFullscreen) {
    is_fullscreen = !g_bWantToBeFullscreen;
  }

  if (g_bWantToBeFullscreen == is_fullscreen) return true;

  if (g_bWantToBeFullscreen) {
    hr = g_spWMPPlayer->put_uiMode(bstr_t{L"none"});

    if (SUCCEEDED(hr)) {
      hr = g_spWMPPlayer->put_fullScreen(VARIANT_TRUE);
    }

    if (SUCCEEDED(hr) && wmp_player2) {
      hr = wmp_player2->put_stretchToFit(VARIANT_TRUE);
    }

    while (ShowCursor(FALSE) >= 0)
      ;
  } else {
    hr = g_spWMPPlayer->put_fullScreen(VARIANT_FALSE);

    if (SUCCEEDED(hr)) {
      hr = g_spWMPPlayer->put_uiMode(bstr_t{L"full"});
    }

    if (SUCCEEDED(hr) && wmp_player2) {
      hr = wmp_player2->put_stretchToFit(
          g_desiredVideoScaleMode == ID_STRETCH_TO_FIT ? VARIANT_TRUE
                                                       : VARIANT_FALSE);
    }

    g_pFrame->ShowWindow(SW_RESTORE);

    while (ShowCursor(TRUE) < 0)
      ;
  }

  is_fullscreen = IsFullScreen();

  if (is_fullscreen != g_bWantToBeFullscreen) {
    g_bWantToBeFullscreen = is_fullscreen;
    OutputDebugString(_T("SetFullScreen FAILED!\n"));
    return false;
  }

  if (SUCCEEDED(hr) && wmp_player2) {
    VARIANT_BOOL stretch = VARIANT_TRUE;

    if (g_bWantToBeFullscreen) {
      hr = wmp_player2->get_stretchToFit(&stretch);

      if (FAILED(hr) || stretch != VARIANT_TRUE) {
        OutputDebugString(_T("SetFullScreen FAILED to set stretchToFit!\n"));
        return false;
      }
    }
  }

  if (!is_fullscreen) {
    ShowCursor(TRUE);
  }

  return true;
}

bool IsVideoPlaying() {
  WMPPlayState playState;
  return SUCCEEDED(g_spWMPPlayer->get_playState(&playState)) &&
         playState == wmppsPlaying;
}

HRESULT PlayVideo(bool bPlay) {
  CComPtr<IWMPControls> wmp_controls;
  HRESULT hr = g_spWMPPlayer->get_controls(&wmp_controls);

  if (SUCCEEDED(hr) && wmp_controls) {
    hr = bPlay ? wmp_controls->play() : wmp_controls->pause();
  }

  return hr;
}

LRESULT SteamWMPWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                 BOOL& bHandled) {
  AtlAxWinInit();
  CComPtr<IAxWinHostWindow> spHost;
  CComPtr<IConnectionPointContainer> spConnectionContainer;
  CComWMPEventDispatch* pEventListener = nullptr;
  CComPtr<IWMPEvents> spEventListener;
  CComPtr<IWMPSettings> spWMPSettings;
  HRESULT hr;
  RECT rcClient;

  advise_cookie_ = 0;
  popup_menu_ = 0;

  // create window
  GetClientRect(&rcClient);
  HWND hwnd =
      window_.Create(m_hWnd, rcClient, nullptr,
                     WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
  if (!hwnd) goto FAILURE;

  // load OCX in window
  hr = window_.QueryHost(&spHost);
  if (ShowFailureMessage(hr)) goto FAILURE;

  hr = spHost->CreateControl(
      CComBSTR(_T("{6BF52A52-394A-11d3-B153-00C04F79FAA6}")), window_, 0);
  if (ShowFailureMessage(hr)) goto FAILURE;

  hr = window_.QueryControl(&g_spWMPPlayer);
  if (ShowFailureMessage(hr)) goto FAILURE;

  // start listening to events
  hr = CComWMPEventDispatch::CreateInstance(&pEventListener);
  spEventListener = pEventListener;
  if (ShowFailureMessage(hr)) goto FAILURE;

  hr = g_spWMPPlayer->QueryInterface(&spConnectionContainer);
  if (ShowFailureMessage(hr)) goto FAILURE;

  // See if OCX supports the IWMPEvents interface
  hr = spConnectionContainer->FindConnectionPoint(__uuidof(IWMPEvents),
                                                  &connection_point_);
  if (FAILED(hr)) {
    // If not, try the _WMPOCXEvents interface, which will use IDispatch
    hr = spConnectionContainer->FindConnectionPoint(__uuidof(_WMPOCXEvents),
                                                    &connection_point_);
    if (ShowFailureMessage(hr)) goto FAILURE;
  }

  hr = connection_point_->Advise(spEventListener, &advise_cookie_);
  if (ShowFailureMessage(hr)) goto FAILURE;

  hr = g_spWMPPlayer->get_settings(&spWMPSettings);
  if (ShowFailureMessage(hr)) goto FAILURE;

  hr = spWMPSettings->put_volume(100);
  if (ShowFailureMessage(hr)) goto FAILURE;

  hr = g_spWMPPlayer->put_enableContextMenu(VARIANT_FALSE);
  if (ShowFailureMessage(hr)) goto FAILURE;

  // set the url of the movie
  hr = g_spWMPPlayer->put_URL(bstr_t{g_URL.c_str()});
  if (ShowFailureMessage(hr)) goto FAILURE;

  return 0;

FAILURE:
  OutputDebugString(_T("SteamWMPWindow::OnCreate FAILED!\n"));
  DestroyWindow();

  if (g_hBlackFadingWindow) {
    ::DestroyWindow(g_hBlackFadingWindow);
  }
  return 0;
}

LRESULT SteamWMPWindow::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                BOOL& bHandled) {
  LogPlayerEvent(ET_CLOSE);

  DestroyWindow();

  if (g_hBlackFadingWindow) {
    ::DestroyWindow(g_hBlackFadingWindow);
  }

  return 0;
}

LRESULT SteamWMPWindow::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  BOOL& bHandled) {
  // stop listening to events
  if (connection_point_) {
    if (0 != advise_cookie_) connection_point_->Unadvise(advise_cookie_);
    connection_point_.Release();
  }

  // close the OCX
  if (g_spWMPPlayer) {
    g_spWMPPlayer->close();
    g_spWMPPlayer.Release();
  }

  m_hWnd = nullptr;
  g_bFrameCreated = false;

  bHandled = FALSE;

  return 1;
}

LRESULT SteamWMPWindow::OnErase(UINT /* uMsg */, WPARAM /* wParam */,
                                LPARAM /* lParam */, BOOL& bHandled) {
  if (g_bWantToBeFullscreen && !IsFullScreen()) {
    g_pFrame->BringWindowToTop();
    SetFullScreen(false);
  }

  bHandled = TRUE;
  return 0;
}

LRESULT SteamWMPWindow::OnSize(UINT /* uMsg */, WPARAM wParam, LPARAM lParam,
                               BOOL& /* lResult */) {
  if (wParam == SIZE_MAXIMIZED) {
    SetFullScreen(true);
  } else {
    if (wParam == SIZE_MINIMIZED) {
      LogPlayerEvent(ET_MINIMIZE);
      if (IsVideoPlaying()) {
        g_bPlayOnRestore = true;
        PlayVideo(false);
      }
    } else if (wParam == SIZE_RESTORED) {
      LogPlayerEvent(ET_RESTORE);
    }

    RECT rcClient;
    GetClientRect(&rcClient);
    window_.MoveWindow(rcClient.left, rcClient.top,
                       rcClient.right - rcClient.left,
                       rcClient.bottom - rcClient.top);
  }

  return 0;
}

LRESULT SteamWMPWindow::OnContextMenu(UINT /* uMsg */, WPARAM /* wParam */,
                                      LPARAM lParam, BOOL& is_handled) {
  if (!popup_menu_) {
    popup_menu_ = CreatePopupMenu();
    // AppendMenuW(popup_menu_, MF_STRING, ID_HALF_SIZE, _T("Zoom 50%"));
    AppendMenuW(popup_menu_, MF_STRING, ID_FULL_SIZE, _T("Zoom 100%"));
    // AppendMenuW(popup_menu_, MF_STRING, ID_DOUBLE_SIZE, _T("Zoom 200%"));
    AppendMenuW(popup_menu_, MF_STRING, ID_STRETCH_TO_FIT,
                _T("Stretch to fit window"));
  }

  TrackPopupMenu(popup_menu_, TPM_LEFTALIGN | TPM_TOPALIGN,
                 GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, m_hWnd,
                 nullptr);

  return 0;
}

LRESULT SteamWMPWindow::OnClick(UINT /* uMsg */, WPARAM /* wParam */,
                                LPARAM /* lParam */, BOOL& /* lResult */) {
  if (IsFullScreen()) SetFullScreen(false);

  return 1;
}

LRESULT SteamWMPWindow::OnLeftDoubleClick(UINT /* uMsg */, WPARAM /* wParam */,
                                          LPARAM /* lParam */,
                                          BOOL& /* lResult */) {
  SetFullScreen(!IsFullScreen());
  return 1;
}

LRESULT SteamWMPWindow::OnSysKeyDown(UINT /* uMsg */, WPARAM wParam,
                                     LPARAM /* lParam */, BOOL& /* lResult */) {
  switch (wParam) {
    case VK_RETURN:
      SetFullScreen(!IsFullScreen());
      break;
  }

  return 1;
}

LRESULT SteamWMPWindow::OnKeyDown(UINT /* uMsg */, WPARAM wParam,
                                  LPARAM /* lParam */, BOOL& /* lResult */) {
  switch (wParam) {
    case VK_SPACE:
      PlayVideo(!IsVideoPlaying());
      break;

    case VK_LEFT:
    case VK_RIGHT: {
      CComPtr<IWMPControls> spWMPControls;
      g_spWMPPlayer->get_controls(&spWMPControls);
      if (!spWMPControls) break;

      CComPtr<IWMPControls2> spWMPControls2;
      spWMPControls.QueryInterface(&spWMPControls2);
      if (!spWMPControls2) break;

      spWMPControls2->step(wParam == VK_LEFT ? -1 : 1);

      LogPlayerEvent(wParam == VK_LEFT ? ET_STEPBCK : ET_STEPFWD);
    } break;

    case VK_UP:
    case VK_DOWN: {
      CComPtr<IWMPControls> spWMPControls;
      g_spWMPPlayer->get_controls(&spWMPControls);
      if (!spWMPControls) break;

      double curpos = 0.0f;
      if (spWMPControls->get_currentPosition(&curpos) != S_OK) break;

      curpos += wParam == VK_UP ? -5.0f : 5.0f;

      spWMPControls->put_currentPosition(curpos);
      if (!IsVideoPlaying()) {
        spWMPControls->play();
        spWMPControls->pause();
      }

      LogPlayerEvent(wParam == VK_UP ? ET_JUMPBCK : ET_JUMPFWD);
    } break;

    case VK_HOME: {
      CComPtr<IWMPControls> spWMPControls;
      g_spWMPPlayer->get_controls(&spWMPControls);
      if (!spWMPControls) break;

      spWMPControls->put_currentPosition(0.0f);
      if (!IsVideoPlaying()) {
        spWMPControls->play();
        spWMPControls->pause();
      }

      LogPlayerEvent(ET_JUMPHOME);
    } break;

    case VK_END: {
      CComPtr<IWMPMedia> spWMPMedia;
      g_spWMPPlayer->get_currentMedia(&spWMPMedia);
      if (!spWMPMedia) break;

      CComPtr<IWMPControls> spWMPControls;
      g_spWMPPlayer->get_controls(&spWMPControls);
      if (!spWMPControls) break;

      double duration = 0.0f;
      if (spWMPMedia->get_duration(&duration) != S_OK) break;

      spWMPControls->put_currentPosition(
          duration - 0.050);  // back a little more than a frame - this avoids
                              // triggering the end fade
      spWMPControls->play();
      spWMPControls->pause();

      LogPlayerEvent(ET_JUMPEND);
    } break;

    case VK_ESCAPE:
      if (IsFullScreen() && !g_bFadeWindowTriggered) {
        CComPtr<IWMPControls> spWMPControls;
        g_spWMPPlayer->get_controls(&spWMPControls);
        if (spWMPControls) {
          spWMPControls->stop();
        }

        LogPlayerEvent(ET_FADEOUT);

        g_bFadeWindowTriggered = true;
        ShowFadeWindow(true);
      }
      break;
  }

  return 0;
}

LRESULT SteamWMPWindow::OnNCActivate(UINT /* uMsg */, WPARAM wParam,
                                     LPARAM /* lParam */, BOOL& /* lResult */) {
  if (wParam) {
    if (g_bWantToBeFullscreen) {
      SetFullScreen(true);
    }
    if (g_bPlayOnRestore) {
      g_bPlayOnRestore = false;
      PlayVideo(true);
    }
  }

  return 1;
}

LRESULT SteamWMPWindow::OnVideoScale(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                     BOOL& bHandled) {
  SetVideoScaleMode(wID);
  return 0;
}
