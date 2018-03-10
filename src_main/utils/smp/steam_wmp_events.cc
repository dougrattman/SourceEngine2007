// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "steam_wmp_events.h"

#include "steam_wmp_window.h"

extern HWND g_hBlackFadingWindow;
extern CComPtr<IWMPPlayer> g_spWMPPlayer;
extern SteamWMPWindow* g_pFrame;
extern double g_timeAtFadeStart;
extern bool g_bFadeIn;

bool g_bFadeWindowTriggered = false;

bool IsFullScreen();
bool SetFullScreen(bool bWantToBeFullscreen);
bool IsVideoPlaying();
void PlayVideo(bool bPlay);
bool ShowFadeWindow(bool bShow);
void LogPlayerEvent(EventType e, double pos);
HRESULT LogPlayerEvent(EventType e);

HRESULT SteamWMPEvents::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                               WORD wFlags, DISPPARAMS FAR* pDispParams,
                               VARIANT FAR* pVarResult,
                               EXCEPINFO FAR* pExcepInfo,
                               unsigned int FAR* puArgErr) {
  if (!pDispParams) return E_POINTER;

  if (pDispParams->cNamedArgs != 0) return DISP_E_NONAMEDARGS;

  HRESULT hr = DISP_E_MEMBERNOTFOUND;

  switch (dispIdMember) {
    case DISPID_WMPCOREEVENT_OPENSTATECHANGE:
      OpenStateChange(pDispParams->rgvarg[0].lVal /* NewState */);
      break;

    case DISPID_WMPCOREEVENT_PLAYSTATECHANGE:
      PlayStateChange(pDispParams->rgvarg[0].lVal /* NewState */);
      break;

    case DISPID_WMPCOREEVENT_AUDIOLANGUAGECHANGE:
      AudioLanguageChange(pDispParams->rgvarg[0].lVal /* LangID */);
      break;

    case DISPID_WMPCOREEVENT_STATUSCHANGE:
      StatusChange();
      break;

    case DISPID_WMPCOREEVENT_SCRIPTCOMMAND:
      ScriptCommand(pDispParams->rgvarg[1].bstrVal /* scType */,
                    pDispParams->rgvarg[0].bstrVal /* Param */);
      break;

    case DISPID_WMPCOREEVENT_NEWSTREAM:
      NewStream();
      break;

    case DISPID_WMPCOREEVENT_DISCONNECT:
      Disconnect(pDispParams->rgvarg[0].lVal /* Result */);
      break;

    case DISPID_WMPCOREEVENT_BUFFERING:
      Buffering(pDispParams->rgvarg[0].boolVal /* Start */);
      break;

    case DISPID_WMPCOREEVENT_ERROR:
      Error();
      break;

    case DISPID_WMPCOREEVENT_WARNING:
      Warning(pDispParams->rgvarg[1].lVal /* WarningType */,
              pDispParams->rgvarg[0].lVal /* Param */,
              pDispParams->rgvarg[2].bstrVal /* Description */);
      break;

    case DISPID_WMPCOREEVENT_ENDOFSTREAM:
      EndOfStream(pDispParams->rgvarg[0].lVal /* Result */);
      break;

    case DISPID_WMPCOREEVENT_POSITIONCHANGE:
      PositionChange(pDispParams->rgvarg[1].dblVal /* oldPosition */,
                     pDispParams->rgvarg[0].dblVal /* newPosition */);
      break;

    case DISPID_WMPCOREEVENT_MARKERHIT:
      MarkerHit(pDispParams->rgvarg[0].lVal /* MarkerNum */);
      break;

    case DISPID_WMPCOREEVENT_DURATIONUNITCHANGE:
      DurationUnitChange(pDispParams->rgvarg[0].lVal /* NewDurationUnit */);
      break;

    case DISPID_WMPCOREEVENT_CDROMMEDIACHANGE:
      CdromMediaChange(pDispParams->rgvarg[0].lVal /* CdromNum */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCHANGE:
      PlaylistChange(
          pDispParams->rgvarg[1].pdispVal /* Playlist */,
          (WMPPlaylistChangeEventType)pDispParams->rgvarg[0].lVal /* change */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTPLAYLISTCHANGE:
      CurrentPlaylistChange(
          (WMPPlaylistChangeEventType)pDispParams->rgvarg[0].lVal /* change */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTPLAYLISTITEMAVAILABLE:
      CurrentPlaylistItemAvailable(
          pDispParams->rgvarg[0].bstrVal /*  bstrItemName */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACHANGE:
      MediaChange(pDispParams->rgvarg[0].pdispVal /* Item */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTMEDIAITEMAVAILABLE:
      CurrentMediaItemAvailable(
          pDispParams->rgvarg[0].bstrVal /* bstrItemName */);
      break;

    case DISPID_WMPCOREEVENT_CURRENTITEMCHANGE:
      CurrentItemChange(pDispParams->rgvarg[0].pdispVal /* pdispMedia */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONCHANGE:
      MediaCollectionChange();
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONATTRIBUTESTRINGADDED:
      MediaCollectionAttributeStringAdded(
          pDispParams->rgvarg[1].bstrVal /* bstrAttribName */,
          pDispParams->rgvarg[0].bstrVal /* bstrAttribVal */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONATTRIBUTESTRINGREMOVED:
      MediaCollectionAttributeStringRemoved(
          pDispParams->rgvarg[1].bstrVal /* bstrAttribName */,
          pDispParams->rgvarg[0].bstrVal /* bstrAttribVal */);
      break;

    case DISPID_WMPCOREEVENT_MEDIACOLLECTIONATTRIBUTESTRINGCHANGED:
      MediaCollectionAttributeStringChanged(
          pDispParams->rgvarg[2].bstrVal /* bstrAttribName */,
          pDispParams->rgvarg[1].bstrVal /* bstrOldAttribVal */,
          pDispParams->rgvarg[0].bstrVal /* bstrNewAttribVal */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONCHANGE:
      PlaylistCollectionChange();
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONPLAYLISTADDED:
      PlaylistCollectionPlaylistAdded(
          pDispParams->rgvarg[0].bstrVal /* bstrPlaylistName */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONPLAYLISTREMOVED:
      PlaylistCollectionPlaylistRemoved(
          pDispParams->rgvarg[0].bstrVal /* bstrPlaylistName */);
      break;

    case DISPID_WMPCOREEVENT_PLAYLISTCOLLECTIONPLAYLISTSETASDELETED:
      PlaylistCollectionPlaylistSetAsDeleted(
          pDispParams->rgvarg[1].bstrVal /* bstrPlaylistName */,
          pDispParams->rgvarg[0].boolVal /* varfIsDeleted */);
      break;

    case DISPID_WMPCOREEVENT_MODECHANGE:
      ModeChange(pDispParams->rgvarg[1].bstrVal /* ModeName */,
                 pDispParams->rgvarg[0].boolVal /* NewValue */);
      break;

    case DISPID_WMPCOREEVENT_MEDIAERROR:
      MediaError(pDispParams->rgvarg[0].pdispVal /* pMediaObject */);
      break;

    case DISPID_WMPCOREEVENT_OPENPLAYLISTSWITCH:
      OpenPlaylistSwitch(pDispParams->rgvarg[0].pdispVal /* pItem */);
      break;

    case DISPID_WMPCOREEVENT_DOMAINCHANGE:
      DomainChange(pDispParams->rgvarg[0].bstrVal /* strDomain */);
      break;

    case DISPID_WMPOCXEVENT_SWITCHEDTOPLAYERAPPLICATION:
      SwitchedToPlayerApplication();
      break;

    case DISPID_WMPOCXEVENT_SWITCHEDTOCONTROL:
      SwitchedToControl();
      break;

    case DISPID_WMPOCXEVENT_PLAYERDOCKEDSTATECHANGE:
      PlayerDockedStateChange();
      break;

    case DISPID_WMPOCXEVENT_PLAYERRECONNECT:
      PlayerReconnect();
      break;

    case DISPID_WMPOCXEVENT_CLICK:
      Click(pDispParams->rgvarg[3].iVal /* nButton */,
            pDispParams->rgvarg[2].iVal /* nShiftState */,
            pDispParams->rgvarg[1].lVal /* fX */,
            pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_DOUBLECLICK:
      DoubleClick(pDispParams->rgvarg[3].iVal /* nButton */,
                  pDispParams->rgvarg[2].iVal /* nShiftState */,
                  pDispParams->rgvarg[1].lVal /* fX */,
                  pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_KEYDOWN:
      KeyDown(pDispParams->rgvarg[1].iVal /* nKeyCode */,
              pDispParams->rgvarg[0].iVal /* nShiftState */);
      break;

    case DISPID_WMPOCXEVENT_KEYPRESS:
      KeyPress(pDispParams->rgvarg[0].iVal /* nKeyAscii */);
      break;

    case DISPID_WMPOCXEVENT_KEYUP:
      KeyUp(pDispParams->rgvarg[1].iVal /* nKeyCode */,
            pDispParams->rgvarg[0].iVal /* nShiftState */);
      break;

    case DISPID_WMPOCXEVENT_MOUSEDOWN:
      MouseDown(pDispParams->rgvarg[3].iVal /* nButton */,
                pDispParams->rgvarg[2].iVal /* nShiftState */,
                pDispParams->rgvarg[1].lVal /* fX */,
                pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_MOUSEMOVE:
      MouseMove(pDispParams->rgvarg[3].iVal /* nButton */,
                pDispParams->rgvarg[2].iVal /* nShiftState */,
                pDispParams->rgvarg[1].lVal /* fX */,
                pDispParams->rgvarg[0].lVal /* fY */);
      break;

    case DISPID_WMPOCXEVENT_MOUSEUP:
      MouseUp(pDispParams->rgvarg[3].iVal /* nButton */,
              pDispParams->rgvarg[2].iVal /* nShiftState */,
              pDispParams->rgvarg[1].lVal /* fX */,
              pDispParams->rgvarg[0].lVal /* fY */);
      break;
  }

  return hr;
}

// Sent when the control changes OpenState
void SteamWMPEvents::OpenStateChange(long NewState) {}

// Sent when the control changes PlayState
void SteamWMPEvents::PlayStateChange(long NewState) {
  WMPPlayState playstate;
  if (g_spWMPPlayer->get_playState(&playstate) == S_OK) {
    switch (playstate) {
      case wmppsPlaying: {
        static bool s_first = true;
        if (s_first) {
          s_first = false;

          LogPlayerEvent(ET_MEDIABEGIN);

          SetFullScreen(true);
          ShowFadeWindow(false);
        } else {
          LogPlayerEvent(ET_PLAY);
        }
        break;
      }

      case wmppsPaused:
        LogPlayerEvent(ET_PAUSE);
        break;

      case wmppsStopped:
        LogPlayerEvent(ET_STOP);
        break;

      case wmppsMediaEnded:
        LogPlayerEvent(ET_MEDIAEND);

        if (IsFullScreen() && !g_bFadeWindowTriggered) {
          g_bFadeWindowTriggered = true;
          ShowFadeWindow(true);
        }
        break;
    }
  }
}

// Sent when the audio language changes
void SteamWMPEvents::AudioLanguageChange(long LangID) {}

// Sent when the status string changes
void SteamWMPEvents::StatusChange() {}

// Sent when a synchronized command or URL is received
void SteamWMPEvents::ScriptCommand(BSTR scType, BSTR Param) {}

// Sent when a new stream is encountered (obsolete)
void SteamWMPEvents::NewStream() {}

// Sent when the control is disconnected from the server (obsolete)
void SteamWMPEvents::Disconnect(long Result) {}

// Sent when the control begins or ends buffering
void SteamWMPEvents::Buffering(VARIANT_BOOL Start) {}

// Sent when the control has an error condition
void SteamWMPEvents::Error() {}

// Sent when the control has an warning condition (obsolete)
void SteamWMPEvents::Warning(long WarningType, long Param, BSTR Description) {}

// Sent when the media has reached end of stream
void SteamWMPEvents::EndOfStream(long Result) {}

// Indicates that the current position of the movie has changed
void SteamWMPEvents::PositionChange(double oldPosition, double newPosition) {
  LogPlayerEvent(ET_SCRUBFROM, (float)oldPosition);
  LogPlayerEvent(ET_SCRUBTO, (float)newPosition);
}

// Sent when a marker is reached
void SteamWMPEvents::MarkerHit(long MarkerNum) {}

// Indicates that the unit used to express duration and position has changed
void SteamWMPEvents::DurationUnitChange(long NewDurationUnit) {}

// Indicates that the CD ROM media has changed
void SteamWMPEvents::CdromMediaChange(long CdromNum) {}

// Sent when a playlist changes
void SteamWMPEvents::PlaylistChange(IDispatch* Playlist,
                                    WMPPlaylistChangeEventType change) {}

// Sent when the current playlist changes
void SteamWMPEvents::CurrentPlaylistChange(WMPPlaylistChangeEventType change) {}

// Sent when a current playlist item becomes available
void SteamWMPEvents::CurrentPlaylistItemAvailable(BSTR bstrItemName) {}

// Sent when a media object changes
void SteamWMPEvents::MediaChange(IDispatch* Item) {}

// Sent when a current media item becomes available
void SteamWMPEvents::CurrentMediaItemAvailable(BSTR bstrItemName) {}

// Sent when the item selection on the current playlist changes
void SteamWMPEvents::CurrentItemChange(IDispatch* pdispMedia) {}

// Sent when the media collection needs to be requeried
void SteamWMPEvents::MediaCollectionChange() {}

// Sent when an attribute string is added in the media collection
void SteamWMPEvents::MediaCollectionAttributeStringAdded(BSTR bstrAttribName,
                                                         BSTR bstrAttribVal) {}

// Sent when an attribute string is removed from the media collection
void SteamWMPEvents::MediaCollectionAttributeStringRemoved(BSTR bstrAttribName,
                                                           BSTR bstrAttribVal) {
}

// Sent when an attribute string is changed in the media collection
void SteamWMPEvents::MediaCollectionAttributeStringChanged(
    BSTR bstrAttribName, BSTR bstrOldAttribVal, BSTR bstrNewAttribVal) {}

// Sent when playlist collection needs to be requeried
void SteamWMPEvents::PlaylistCollectionChange() {}

// Sent when a playlist is added to the playlist collection
void SteamWMPEvents::PlaylistCollectionPlaylistAdded(BSTR bstrPlaylistName) {}

// Sent when a playlist is removed from the playlist collection
void SteamWMPEvents::PlaylistCollectionPlaylistRemoved(BSTR bstrPlaylistName) {}

// Sent when a playlist has been set or reset as deleted
void SteamWMPEvents::PlaylistCollectionPlaylistSetAsDeleted(
    BSTR bstrPlaylistName, VARIANT_BOOL varfIsDeleted) {}

// Playlist playback mode has changed
void SteamWMPEvents::ModeChange(BSTR ModeName, VARIANT_BOOL NewValue) {}

// Sent when the media object has an error condition
void SteamWMPEvents::MediaError(IDispatch* media_object) {
  while (ShowCursor(TRUE) < 0)
    ;

  ShowWindow(g_hBlackFadingWindow, SW_HIDE);
  if (g_pFrame) {
    g_pFrame->ShowWindow(SW_HIDE);
  }

  CComPtr<IWMPMedia> wmp_media;
  if (media_object) {
    media_object->QueryInterface(&wmp_media);
  }

  if (wmp_media) {
    BSTR source_url;
    HRESULT hr = wmp_media->get_sourceURL(&source_url);

    wchar_t error_message[1024];
    wsprintfW(error_message, _T("Unable to open media: %s\n"),
              SUCCEEDED(hr) ? CW2T(source_url).operator wchar_t*() : _T("N/A"));

    MessageBoxW(nullptr, error_message, _T("Steam Media Player - Media Error"),
                MB_OK | MB_ICONERROR);

    SysFreeString(source_url);
  } else {
    MessageBoxW(nullptr, _T("Media Error"),
                _T("Steam Media Player - Media Error"), MB_OK | MB_ICONERROR);
  }

  g_pFrame->PostMessageW(WM_CLOSE);
}

// Current playlist switch with no open state change
void SteamWMPEvents::OpenPlaylistSwitch(IDispatch* pItem) {}

// Sent when the current DVD domain changes
void SteamWMPEvents::DomainChange(BSTR strDomain) {}

// Sent when display switches to player application
void SteamWMPEvents::SwitchedToPlayerApplication() {}

// Sent when display switches to control
void SteamWMPEvents::SwitchedToControl() {}

// Sent when the player docks or undocks
void SteamWMPEvents::PlayerDockedStateChange() {}

// Sent when the OCX reconnects to the player
void SteamWMPEvents::PlayerReconnect() {}

// Occurs when a user clicks the mouse
void SteamWMPEvents::Click(short nButton, short nShiftState, long fX, long fY) {
  if (IsFullScreen()) {
    SetFullScreen(false);
  }
}

// Occurs when a user double-clicks the mouse
void SteamWMPEvents::DoubleClick(short nButton, short nShiftState, long fX,
                                 long fY) {
  // the controls are just drawn into the main window, wheras the video has its
  // own window this check allows us to only fullscreen on doubleclick within
  // the video area
  POINT pt = {fX, fY};
  HWND hWnd = g_pFrame->ChildWindowFromPoint(pt);
  if (ChildWindowFromPoint(hWnd, pt) != hWnd) {
    SetFullScreen(!IsFullScreen());
  }
}

// Occurs when a key is pressed
void SteamWMPEvents::KeyDown(short nKeyCode, short nShiftState) {
  if (!g_pFrame) return;

  BOOL rval;
  // 4 is the alt keymask
  if (nShiftState & 4) {
    g_pFrame->OnSysKeyDown(WM_KEYDOWN, nKeyCode, 0, rval);
  } else {
    g_pFrame->OnKeyDown(WM_KEYDOWN, nKeyCode, 0, rval);
  }
}

// Occurs when a key is pressed and released
void SteamWMPEvents::KeyPress(short nKeyAscii) {}

// Occurs when a key is released
void SteamWMPEvents::KeyUp(short nKeyCode, short nShiftState) {}

// Occurs when a mouse button is pressed
void SteamWMPEvents::MouseDown(short nButton, short nShiftState, long fX,
                               long fY) {}

// Occurs when a mouse pointer is moved
void SteamWMPEvents::MouseMove(short nButton, short nShiftState, long fX,
                               long fY) {}

// Occurs when a mouse button is released
void SteamWMPEvents::MouseUp(short nButton, short nShiftState, long fX,
                             long fY) {}
