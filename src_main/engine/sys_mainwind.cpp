// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "sys_mainwind.h"

#include "base/include/windows/scoped_winsock_initializer.h"
#include "base/include/windows/windows_light.h"

#include "avi/iavi.h"
#include "cdll_engine_int.h"
#include "cl_main.h"
#include "deps/bink/bink.h"
#include "gameui/igameui.h"
#include "gl_matsysiface.h"
#include "host.h"
#include "iengine.h"
#include "igame.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/iinputsystem.h"
#include "ivideomode.h"
#include "keys.h"
#include "matchmaking.h"
#include "quakedef.h"
#include "sound.h"
#include "sv_main.h"
#include "sys_dll.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"
#include "tier3/tier3.h"
#include "vgui_baseui_interface.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/messagedialog.h"
#include "vguimatsurface/imatsystemsurface.h"

#include "tier0/include/memdbgon.h"

void S_BlockSound();
void S_UnblockSound();
void ClearIOStates();

class CGame;

// In VCR playback mode, it sleeps this amount each frame.
int g_iVCRPlaybackSleepInterval{0};
// During VCR playback, if this is true, then it'll pause at the end of each
// frame.
bool g_bVCRSingleStep{false};
// Used to prevent it from running frames while you hold the S key down.
bool g_bWaitingForStepKeyUp{false};
bool g_bShowVCRPlaybackDisplay{true};

void VCR_EnterPausedState() {
  // Turn this off in case they're in single-step mode.
  g_bVCRSingleStep = false;

  // This is cheesy, but GetAsyncKeyState is blocked (in protected_things.h)
  // from being accidentally used, so we get it through
#undef GetAsyncKeyState

  const int is_key_down_mask{0x8000};

  // In this mode, we enter a wait state where we only pay attention to R and Q.
  while (1) {
    if (GetAsyncKeyState('R') & is_key_down_mask) break;

    if (GetAsyncKeyState('Q') & is_key_down_mask)
      TerminateProcess(GetCurrentProcess(), 1);

    if (GetAsyncKeyState('S') & is_key_down_mask) {
      if (!g_bWaitingForStepKeyUp) {
        // Do a single step.
        g_bVCRSingleStep = true;
        g_bWaitingForStepKeyUp =
            true;  // Don't do another single step until they release the S key.
        break;
      }
    } else {
      // Ok, they released the S key, so we'll process it next time the key goes
      // down.
      g_bWaitingForStepKeyUp = false;
    }

    Sleep(2);
  }
}

namespace {
// Game input events
enum GameInputEventType_t {
  IE_Close = IE_FirstAppEvent,
  IE_WindowMove,
  IE_AppActivated,
};

void DoSomeSocketStuffInOrderToGetZoneAlarmToNoticeUs() {
#ifdef OS_WIN
  source::windows::ScopedWinsockInitializer scoped_winsock_initializer{
      source::windows::WinsockVersion::Version2_2};
  DWORD error_code = scoped_winsock_initializer.error_code();
  if (error_code != ERROR_SUCCESS) {
    Warning("Winsock 2.2 unavailable (0x%.8x).", error_code);
    return;
  }

  SOCKET temp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (temp_socket == INVALID_SOCKET) {
    return;
  }

  char options[] = {1};
  setsockopt(temp_socket, SOL_SOCKET, SO_BROADCAST, options,
             SOURCE_ARRAYSIZE(options));

  char host_name[256];
  gethostname(host_name, SOURCE_ARRAYSIZE(host_name));

  hostent *hInfo = gethostbyname(host_name);
  if (hInfo) {
    sockaddr_in myIpAddress{0};
    myIpAddress.sin_family = AF_INET;
    myIpAddress.sin_port = htons(27015);  // our normal server port
    myIpAddress.sin_addr.S_un.S_un_b.s_b1 = hInfo->h_addr_list[0][0];
    myIpAddress.sin_addr.S_un.S_un_b.s_b2 = hInfo->h_addr_list[0][1];
    myIpAddress.sin_addr.S_un.S_un_b.s_b3 = hInfo->h_addr_list[0][2];
    myIpAddress.sin_addr.S_un.S_un_b.s_b4 = hInfo->h_addr_list[0][3];

    if (bind(temp_socket, (sockaddr *)&myIpAddress, sizeof(myIpAddress)) !=
        -1) {
      if (sendto(temp_socket, host_name, 1, 0, (sockaddr *)&myIpAddress,
                 sizeof(myIpAddress)) == -1) {
        // error?
      }
    }
  }
  closesocket(temp_socket);
#endif
}

HICON LoadGameWindowIcon(IFileSystem *file_system) {
  char local_icon_path[SOURCE_MAX_PATH];
  if (file_system->GetLocalPath("resource/game.ico", local_icon_path,
                                SOURCE_ARRAYSIZE(local_icon_path))) {
    file_system->GetLocalCopy(local_icon_path);
    return (HICON)LoadImageA(nullptr, local_icon_path, IMAGE_ICON, 0, 0,
                             LR_LOADFROMFILE | LR_DEFAULTSIZE);
  }

  constexpr int kDefaultExeIcon{101};
  return LoadIconA(GetModuleHandleW(nullptr), MAKEINTRESOURCE(kDefaultExeIcon));
}

void VCR_HandlePlaybackMessages(HWND hWnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam) {
  if (uMsg == WM_KEYDOWN) {
    if (wParam == VK_SUBTRACT || wParam == 0xbd) {
      g_iVCRPlaybackSleepInterval += 5;
    } else if (wParam == VK_ADD || wParam == 0xbb) {
      g_iVCRPlaybackSleepInterval -= 5;
    } else if (toupper(wParam) == 'Q') {
      TerminateProcess(GetCurrentProcess(), 1);
    } else if (toupper(wParam) == 'P') {
      VCR_EnterPausedState();
    } else if (toupper(wParam) == 'S' && !g_bVCRSingleStep) {
      g_bWaitingForStepKeyUp = true;
      VCR_EnterPausedState();
    } else if (toupper(wParam) == 'D') {
      g_bShowVCRPlaybackDisplay = !g_bShowVCRPlaybackDisplay;
    }

    g_iVCRPlaybackSleepInterval =
        std::clamp(g_iVCRPlaybackSleepInterval, 0, 500);
  }
}

LONG WINAPI SourceEngineWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam);
}  // namespace

// Purpose: Main game interface, including message pump and window creation
class CGame : public IGame {
 public:
  CGame()
      : hwnd_{nullptr},
        instance_{nullptr},
        chained_wnd_proc_{nullptr},
        is_window_active_{false},
        is_external_window_{false},
        window_x_{0},
        window_y_{0},
        window_width_{0},
        window_height_{0},
        desktop_width_{0},
        desktop_height_{0},
        desktop_refresh_rate_{0} {}

  virtual ~CGame() = default;

  bool Init(void *instance) override {
    is_external_window_ = false;
    instance_ = (HINSTANCE)instance;
    return true;
  }

  bool Shutdown() override {
    instance_ = nullptr;
    return true;
  }

  bool CreateGameWindow() override {
#ifndef SWDS
    WNDCLASSW wc{0};
    wc.style = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = &DefWindowProcW;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;
    wc.hIcon = LoadGameWindowIcon(g_pFileSystem);

    // Get the window name
    char window_name[256];
    window_name[0] = 0;
    KeyValues *modinfo{new KeyValues("ModInfo")};
    if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt")) {
      Q_strncpy(window_name, modinfo->GetString("game"),
                SOURCE_ARRAYSIZE(window_name));
    }

    char const *game_type{modinfo->GetString("type")};
    if (game_type && Q_stristr(game_type, "multiplayer"))
      DoSomeSocketStuffInOrderToGetZoneAlarmToNoticeUs();

    modinfo->deleteThis();

    if (!window_name[0]) {
      Q_strncpy(window_name, "HALF-LIFE 2", SOURCE_ARRAYSIZE(window_name));
    }

    wch unicode_window_name[512];
    ::MultiByteToWideChar(CP_UTF8, 0, window_name, -1, unicode_window_name,
                          SOURCE_ARRAYSIZE(unicode_window_name));

    // Oops, we didn't clean up the class registration from last cycle which
    // might mean that the wndproc pointer is bogus
    UnregisterClassW(kWindowClassName, instance_);
    // Register it again
    RegisterClassW(&wc);

    // Note, it's hidden
    DWORD style{WS_POPUP | WS_CLIPSIBLINGS};

    // Give it a frame
    if (videomode->IsWindowedMode()) {
      style |= WS_OVERLAPPEDWINDOW;
      style &= ~WS_THICKFRAME;
    }

    // Never a max box
    style &= ~WS_MAXIMIZEBOX;

    // Create a full screen size window by default, it'll get resized later
    // anyway
    int width{GetSystemMetrics(SM_CXSCREEN)};
    int height{GetSystemMetrics(SM_CYSCREEN)};

    // Create the window
    DWORD ex_style{0};
    if (g_bTextMode) {
      style &= ~WS_VISIBLE;
      // So it doesn't show up in the taskbar.
      ex_style |= WS_EX_TOOLWINDOW;
    }

    const HWND hwnd = CreateWindowExW(
        ex_style, kWindowClassName, unicode_window_name, style, 0, 0, width,
        height, nullptr, nullptr, instance_, nullptr);

    // NOTE: On some cards, CreateWindowExW slams the FPU control word
    SetupFPUControlWord();

    if (!hwnd) {
      Error("Unable to create game window");
      return false;
    }

    SetMainWindow(hwnd);

    AttachToWindow();
    return true;
#else
    return true;
#endif
  }

  void DestroyGameWindow() override {
#ifndef SWDS
    // Destroy all things created when the window was created
    if (!is_external_window_) {
      DetachFromWindow();

      if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
      }

      UnregisterClassW(kWindowClassName, instance_);
    } else {
      hwnd_ = nullptr;
      is_external_window_ = false;
    }
#endif  // !defined( SWDS )
  }

  void SetGameWindow(void *hWnd) override {
    is_external_window_ = true;
    SetMainWindow((HWND)hWnd);
  }

  // This is used in edit mode to override the default wnd proc associated
  bool InputAttachToGameWindow() override {
    // We can't use this feature unless we didn't control the creation of the
    // window
    if (!is_external_window_) return true;

    AttachToWindow();

#ifndef SWDS
    vgui::surface()->OnScreenSizeChanged(videomode->GetModeWidth(),
                                         videomode->GetModeHeight());
#endif

    // We don't get WM_ACTIVATEAPP messages in this case; simulate one.
    AppActivate(true);

    // Capture + hide the mouse
    SetCapture(hwnd_);

    return true;
  }

  void InputDetachFromGameWindow() override {
    // We can't use this feature unless we didn't control the creation of the
    // window
    if (!is_external_window_) return;

    if (!chained_wnd_proc_) return;

    // Release + show the mouse
    ReleaseCapture();

    // We don't get WM_ACTIVATEAPP messages in this case; simulate one.
    AppActivate(false);

    DetachFromWindow();
  }

  void PlayStartupVideos() override {
#ifndef SWDS

    // Wait for the mode to change and stabilized
    // TODO(d.rattman): There's really no way to know when this is completed, so
    // we have to guess a time that will mostly be correct
    if (videomode->IsWindowedMode() == false) {
      Sleep(1000);
    }

    bool bEndGame = CommandLine()->CheckParm("-endgamevid");
    bool bRecap = CommandLine()->CheckParm(
        "-recapvid");  // TODO(d.rattman): This is a temp
                       // addition until the
                       // movie playback is
                       // centralized -- jdw

    if (!bEndGame && !bRecap &&
        (CommandLine()->CheckParm("-dev") ||
         CommandLine()->CheckParm("-novid") ||
         CommandLine()->CheckParm("-allowdebug")))
      return;

    const char *pszFile = "media/StartupVids.txt";
    if (bEndGame) {
      // Don't go back into the map that triggered this.
      CommandLine()->RemoveParm("+map");
      CommandLine()->RemoveParm("+load");

      pszFile = "media/EndGameVids.txt";
    } else if (bRecap) {
      pszFile = "media/RecapVids.txt";
    }

    int vidFileLength;

    // have to use the malloc memory allocation option in COM_LoadFile since the
    // memory system isn't set up at this point.
    const char *buffer = (char *)COM_LoadFile(pszFile, 5, &vidFileLength);

    if ((buffer == nullptr) || (vidFileLength == 0)) {
      return;
    }

    // hide cursor while playing videos
    ::ShowCursor(FALSE);

    const char *start = buffer;

    while (1) {
      start = COM_Parse(start);
      if (Q_strlen(com_token) <= 0) {
        break;
      }

      // get the path to the avi file and play it.
      char localPath[SOURCE_MAX_PATH];
      g_pFileSystem->GetLocalPath(com_token, localPath, sizeof(localPath));
      PlayVideoAndWait(localPath);
      localPath[0] = 0;  // just to make sure we don't play the same avi file
                         // twice in the case that one movie is there but
                         // another isn't.
    }

    // show cursor again
    ::ShowCursor(TRUE);

    // call free on the buffer since the buffer was malloc'd in COM_LoadFile
    free((void *)buffer);

#endif  // SWDS
  }

  void *GetMainWindow() const override { return hwnd_; }

  void **GetMainWindowAddress() const override { return (void **)&hwnd_; }

  std::tuple<i32, i32, i32> GetDesktopInfo() const override {
    // order of initialization means that this might get called early.  In that
    // case go ahead and grab the current screen window and setup based on that.
    // we need to do this when initializing the base list of video modes, for
    // example
    if (desktop_width_ == 0) {
      HDC dc = ::GetDC(nullptr);
      i32 width = ::GetDeviceCaps(dc, HORZRES);
      i32 height = ::GetDeviceCaps(dc, VERTRES);
      i32 refreshrate = ::GetDeviceCaps(dc, VREFRESH);
      ::ReleaseDC(nullptr, dc);

      return {width, height, refreshrate};
    }

    return {desktop_width_, desktop_height_, desktop_refresh_rate_};
  }

  void SetWindowXY(int x, int y) override {
    window_x_ = x;
    window_y_ = y;
  }

  void SetWindowSize(int w, int h) override {
    window_width_ = w;
    window_height_ = h;
  }

  std::tuple<i32, i32, i32, i32> GetWindowRect() const override {
    return {window_x_, window_y_, window_width_, window_height_};
  }

  bool IsActiveApp() const override { return is_window_active_; }

  // Dispatch all the queued up messages.
  void DispatchAllStoredGameMessages() override {
#if !defined(NO_VCR)
    if (VCRGetMode() == VCR_Playback) {
      InputEvent_t event;
      while (VCRHook_PlaybackGameMsg(&event)) {
        event.m_nTick = g_pInputSystem->GetPollTick();
        DispatchInputEvent(event);
      }
    } else {
      int nEventCount = g_pInputSystem->GetEventCount();
      const InputEvent_t *pEvents = g_pInputSystem->GetEventData();
      for (int i = 0; i < nEventCount; ++i) {
        VCRHook_RecordGameMsg(pEvents[i]);
        DispatchInputEvent(pEvents[i]);
      }
      VCRHook_RecordEndGameMsg();
    }
#else
    int nEventCount = g_pInputSystem->GetEventCount();
    const InputEvent_t *pEvents = g_pInputSystem->GetEventData();
    for (int i = 0; i < nEventCount; ++i) {
      DispatchInputEvent(pEvents[i]);
    }
#endif
  }

 public:
  void SetMainWindow(HWND window) {
    hwnd_ = window;

    avi->SetMainWindow(window);

    // update our desktop info (since the results will change if we are going to
    // fullscreen mode)
    if (!desktop_width_ || !desktop_height_) {
      UpdateDesktopInformation(hwnd_);
    }
  }

  void SetActiveApp(bool active) { is_window_active_ = active; }

  int WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // NOTE: the way this function works is to handle all messages that just
    // call through to Windows or provide data to it.
    //
    // Any messages that change the engine's internal state (like key events)
    // are stored in a list and processed at the end of the frame. This is
    // necessary for VCR mode to work correctly because Windows likes to pump
    // messages during some of its API calls like SetWindowPos, and unless we
    // add custom code around every Windows API call so VCR mode can trap the
    // wndproc calls, VCR mode can't reproduce the calls to the wndproc.
    if (eng->GetQuitting() != IEngine::QUIT_NOTQUITTING)
      return CallWindowProc(chained_wnd_proc_, hWnd, uMsg, wParam, lParam);

    // If we're playing back, listen to a couple input things used to drive VCR
    // mode
    if (VCRGetMode() == VCR_Playback) {
      VCR_HandlePlaybackMessages(hWnd, uMsg, wParam, lParam);
    }

    // Note: NO engine state should be changed in here while in VCR record or
    // playback. We can send whatever we want to Windows, but if we change its
    // state in here instead of in DispatchAllStoredGameMessages, the playback
    // may not work because Windows messages are not deterministic, so you might
    // get different messages during playback than you did during record.
    InputEvent_t event{0};
    event.m_nTick = g_pInputSystem->GetPollTick();

    LONG return_code{0};

    switch (uMsg) {
      case WM_CREATE:
        ::SetForegroundWindow(hWnd);
        break;

      case WM_ACTIVATEAPP: {
        bool is_activated = wParam == 1;
        event.m_nType = IE_AppActivated;
        event.m_nData = is_activated;
        g_pInputSystem->PostUserEvent(event);
      } break;

      case WM_POWERBROADCAST:
        // Don't go into Sleep mode when running engine, we crash on resume for
        // some reason (as do half of the apps I have running usually anyway...)
        if (wParam == PBT_APMQUERYSUSPEND) {
          Msg("OS requested hibernation, ignoring request.\n");
          return BROADCAST_QUERY_DENY;
        }

        return_code =
            CallWindowProc(chained_wnd_proc_, hWnd, uMsg, wParam, lParam);
        break;

      case WM_SYSCOMMAND:
        if ((wParam == SC_MONITORPOWER) || (wParam == SC_KEYMENU) ||
            (wParam == SC_SCREENSAVE))
          return return_code;

        if (wParam == SC_CLOSE) {
#if !defined(NO_VCR)
          // handle the close message, but make sure
          // it's not because we accidently hit ALT-F4
          if (HIBYTE(VCRHook_GetKeyState(VK_LMENU)) ||
              HIBYTE(VCRHook_GetKeyState(VK_RMENU)))
            return return_code;
#endif
          Cbuf_Clear();
          Cbuf_AddText("quit\n");
        }

#ifndef SWDS
        if (VCRGetMode() == VCR_Disabled) {
          S_BlockSound();
          S_ClearBuffer();
        }
#endif

        return_code =
            CallWindowProc(chained_wnd_proc_, hWnd, uMsg, wParam, lParam);

#ifndef SWDS
        if (VCRGetMode() == VCR_Disabled) {
          S_UnblockSound();
        }
#endif
        break;

      case WM_CLOSE:
        // Handle close messages
        event.m_nType = IE_Close;
        g_pInputSystem->PostUserEvent(event);
        return 0;

      case WM_MOVE:
        event.m_nType = IE_WindowMove;
        event.m_nData = (short)LOWORD(lParam);
        event.m_nData2 = (short)HIWORD(lParam);
        g_pInputSystem->PostUserEvent(event);
        break;

      case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
          // Update restored client rect
          ::GetClientRect(hWnd, &last_restored_client_rect_);
        } else {
          // Fix the window rect to have same client area as it used to have
          // before it got minimized
          RECT rcWindow;
          ::GetWindowRect(hWnd, &rcWindow);

          rcWindow.right = rcWindow.left + last_restored_client_rect_.right;
          rcWindow.bottom = rcWindow.top + last_restored_client_rect_.bottom;

          ::AdjustWindowRect(&rcWindow, ::GetWindowLong(hWnd, GWL_STYLE),
                             FALSE);
          ::MoveWindow(hWnd, rcWindow.left, rcWindow.top,
                       rcWindow.right - rcWindow.left,
                       rcWindow.bottom - rcWindow.top, FALSE);
        }
        break;

      case WM_SYSCHAR:
        // keep Alt-Space from happening
        break;

      case WM_COPYDATA:
        // Hammer -> engine remote console command.
        // Return true to indicate that the message was handled.
        Cbuf_AddText((const char *)(((COPYDATASTRUCT *)lParam)->lpData));
        Cbuf_AddText("\n");
        return_code = 1;
        break;

      case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT client_rect;
        GetClientRect(hWnd, &client_rect);
#ifndef SWDS
        // Only renders stuff if running -noshaderapi
        if (videomode) {
          videomode->DrawNullBackground(hdc, client_rect.right,
                                        client_rect.bottom);
        }
#endif
        EndPaint(hWnd, &ps);
      } break;

      case WM_DISPLAYCHANGE:
        if (!desktop_height_ || !desktop_width_) {
          UpdateDesktopInformation(wParam, lParam);
        }
        break;

      case WM_IME_NOTIFY:
        switch (wParam) {
          default:
            break;

#ifndef SWDS
          case 14:
            if (!videomode->IsWindowedMode()) return 0;
            break;
#endif
        }
        return CallWindowProc(chained_wnd_proc_, hWnd, uMsg, wParam, lParam);

      default:
        return_code =
            CallWindowProc(chained_wnd_proc_, hWnd, uMsg, wParam, lParam);
        break;
    }

    // return 0 if handled message, 1 if not
    return return_code;
  }

  // Message handlers.
  void HandleMsg_WindowMove(const InputEvent_t &event) {
    game->SetWindowXY(event.m_nData, event.m_nData2);
#ifndef SWDS
    videomode->UpdateWindowPosition();
#endif
  }

  void HandleMsg_ActivateApp(const InputEvent_t &event) {
    AppActivate(event.m_nData ? true : false);
  }

  void HandleMsg_Close(const InputEvent_t &event) {
    if (eng->GetState() == IEngine::DLL_ACTIVE) {
      eng->SetQuitting(IEngine::QUIT_TODESKTOP);
    }
  }

  // Call the appropriate HandleMsg_ function.
  void DispatchInputEvent(const InputEvent_t &event);

 private:
  void AppActivate(bool fActive) {
    // If text mode, force it to be active.
    if (g_bTextMode) {
      fActive = true;
    }

    // Don't bother if we're already in the correct state
    if (IsActiveApp() == fActive) return;

#ifndef SWDS
    if (host_initialized) {
      if (fActive) {
        if (videomode) {
          videomode->RestoreVideo();
        }

        // Clear keyboard states (should be cleared already but...)
        // VGui_ActivateMouse will reactivate the mouse soon.
        ClearIOStates();

        UpdateMaterialSystemConfig();
      } else {
        // Clear keyboard input and deactivate the mouse while we're away.
        ClearIOStates();

        if (g_ClientDLL) {
          g_ClientDLL->IN_DeactivateMouse();
        }

        if (videomode) {
          videomode->ReleaseVideo();
        }
      }
    }
#endif  // SWDS
    SetActiveApp(fActive);
  }

  void PlayVideoAndWait(const char *filename) {
#if defined(_WIN32)
    if (!filename || !filename[0]) return;

    // Black out the back of the screen once at the beginning of each video
    // (since we're not scaling to fit)
    HDC dc = ::GetDC(hwnd_);

    RECT rect;
    rect.top = 0;
    rect.bottom = window_height_;
    rect.left = 0;
    rect.right = window_width_;

    HBRUSH hBlackBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
    ::SetViewportOrgEx(dc, 0, 0, nullptr);
    ::FillRect(dc, &rect, hBlackBrush);
    ::ReleaseDC((HWND)GetMainWindow(), dc);

    // Supplying a nullptr context will cause Bink to allocate its own
    BinkSoundUseDirectSound(nullptr);

    // Open the bink file with audio
    HBINK hBINK = BinkOpen(filename, BINKSNDTRACK);
    if (hBINK == 0) return;

    // Create a buffer to decompress to
    // NOTE: The DIB version is the only one we can call on without DirectDraw
    HBINKBUFFER hBINKBuffer = BinkBufferOpen(
        GetMainWindow(), hBINK->Width, hBINK->Height,
        BINKBUFFERDIBSECTION | BINKBUFFERSTRETCHXINT | BINKBUFFERSTRETCHYINT |
            BINKBUFFERSHRINKXINT | BINKBUFFERSHRINKYINT);
    if (hBINKBuffer == 0) {
      BinkClose(hBINK);
      return;
    }

    // Integral scaling is much faster, so always scale the video as such
    int nNewWidth = (int)hBINK->Width;
    int nNewHeight = (int)hBINK->Height;

    // Find if we need to scale up or down
    if ((int)hBINK->Width < window_width_ &&
        (int)hBINK->Height < window_height_) {
      // Scaling up by powers of two
      int nScale = 0;
      while (((int)hBINK->Width << (nScale + 1)) <= window_width_ &&
             ((int)hBINK->Height << (nScale + 1)) <= window_height_)
        nScale++;

      nNewWidth = (int)hBINK->Width << nScale;
      nNewHeight = (int)hBINK->Height << nScale;
    } else if ((int)hBINK->Width > window_width_ &&
               (int)hBINK->Height > window_height_) {
      // Scaling down by powers of two
      int nScale = 1;
      while (((int)hBINK->Width >> nScale) > window_width_ &&
             ((int)hBINK->Height >> nScale) > window_height_)
        nScale++;

      nNewWidth = (int)hBINK->Width >> nScale;
      nNewHeight = (int)hBINK->Height >> nScale;
    }

    // Scale if we need to
    BinkBufferSetScale(hBINKBuffer, nNewWidth, nNewHeight);
    int nXPos = (window_width_ - nNewWidth) / 2;
    int nYPos = (window_height_ - nNewHeight) / 2;

    // Offset to the middle of the screen
    BinkBufferSetOffset(hBINKBuffer, nXPos, nYPos);

    // We need to be able to poll the state of the input device, but we're not
    // completely setup yet, so this spoofs the ability
    const int is_key_down_mask{0x8000};
    while (true) {
#undef GetAsyncKeyState

      // Escape, return, or space stops the playback
      const short key_state =
          (GetAsyncKeyState(VK_ESCAPE) | GetAsyncKeyState(VK_SPACE) |
           GetAsyncKeyState(VK_RETURN));
      if (key_state & is_key_down_mask) break;

      // Decompress this frame
      BinkDoFrame(hBINK);

      // Lock the buffer for writing
      if (BinkBufferLock(hBINKBuffer)) {
        // Copy the decompressed frame into the BinkBuffer
        BinkCopyToBuffer(hBINK, hBINKBuffer->Buffer, hBINKBuffer->BufferPitch,
                         hBINKBuffer->Height, 0, 0, hBINKBuffer->SurfaceType);

        // Unlock the buffer
        BinkBufferUnlock(hBINKBuffer);
      }

      // Blit the pixels to the screen
      BinkBufferBlit(hBINKBuffer, hBINK->FrameRects,
                     BinkGetRects(hBINK, hBINKBuffer->SurfaceType));

      // Wait until the next frame is ready
      while (BinkWait(hBINK)) {
        // TODO(d.rattman): Bink recommends a higher precision than this
        Sleep(1);
      }

      // Check for video being complete
      if (hBINK->FrameNum == hBINK->Frames) break;

      // Move on
      BinkNextFrame(hBINK);
    }

    // Close it all down
    BinkBufferClose(hBINKBuffer);
    BinkClose(hBINK);
#endif
  }  // plays a video file and waits

  void AttachToWindow() {
    if (!hwnd_) return;

    chained_wnd_proc_ = (WNDPROC)GetWindowLongPtrW(hwnd_, GWLP_WNDPROC);
    SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, (LONG_PTR)SourceEngineWindowProc);

    if (g_pInputSystem) {
      // Attach the input system window proc
      g_pInputSystem->AttachToWindow(hwnd_);
      g_pInputSystem->EnableInput(true);
      g_pInputSystem->EnableMessagePump(false);
    }

    if (g_pMatSystemSurface) {
      // Attach the vgui matsurface window proc
      g_pMatSystemSurface->AttachToWindow(hwnd_, true);
      g_pMatSystemSurface->EnableWindowsMessages(true);
    }
  }

  void DetachFromWindow() {
    if (!hwnd_ || !chained_wnd_proc_) {
      chained_wnd_proc_ = nullptr;
      return;
    }

    if (g_pMatSystemSurface) {
      // Detach the vgui matsurface window proc
      g_pMatSystemSurface->AttachToWindow(nullptr);
    }

    if (g_pInputSystem) {
      // Detach the input system window proc
      g_pInputSystem->EnableInput(false);
      g_pInputSystem->DetachFromWindow();
    }

    Assert((WNDPROC)GetWindowLongPtrW(hwnd_, GWLP_WNDPROC) ==
           SourceEngineWindowProc);
    SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, (LONG_PTR)chained_wnd_proc_);
  }

  void UpdateDesktopInformation(HWND hWnd) {
    HDC dc = ::GetDC(hWnd);
    desktop_width_ = ::GetDeviceCaps(dc, HORZRES);
    desktop_height_ = ::GetDeviceCaps(dc, VERTRES);
    desktop_refresh_rate_ = ::GetDeviceCaps(dc, VREFRESH);
    ::ReleaseDC(hWnd, dc);
  }

  void UpdateDesktopInformation(WPARAM wParam, LPARAM lParam) {
    desktop_width_ = LOWORD(lParam);
    desktop_height_ = HIWORD(lParam);
  }

  static const wch kWindowClassName[];

  HWND hwnd_;
  HINSTANCE instance_;
  // Stores a wndproc to chain message calls to
  WNDPROC chained_wnd_proc_;

  bool is_window_active_, is_external_window_;

  i32 window_x_, window_y_, window_width_, window_height_;
  i32 desktop_width_, desktop_height_, desktop_refresh_rate_;

  RECT last_restored_client_rect_;
};

static CGame g_game;
IGame *game = &g_game;

const wch CGame::kWindowClassName[] = L"Valve001";

namespace {
// These are all the windows messages that can change game state.
// See CGame::WindowProc for a description of how they work.
struct GameMessageHandler {
  void (CGame::*pFn)(const InputEvent_t &inputEvent);
  int eventType;
};

GameMessageHandler game_message_handlers[] = {
    {&CGame::HandleMsg_ActivateApp, IE_AppActivated},
    {&CGame::HandleMsg_WindowMove, IE_WindowMove},
    {&CGame::HandleMsg_Close, IE_Close},
    {&CGame::HandleMsg_Close, IE_Quit},
};

LONG WINAPI SourceEngineWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam) {
  return g_game.WindowProc(hWnd, uMsg, wParam, lParam);
}
}  // namespace

// Call the appropriate HandleMsg_ function.
void CGame::DispatchInputEvent(const InputEvent_t &event) {
  switch (event.m_nType) {
      // Handle button events specially,
      // since we have all manner of crazy filtering going on	when dealing
      // with them
    case IE_ButtonPressed:
    case IE_ButtonDoubleClicked:
    case IE_ButtonReleased:
      Key_Event(event);
      break;

    default:
      // Let vgui have the first whack at events
      if (g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent(event))
        break;

      for (auto &handler : game_message_handlers) {
        if (handler.eventType == event.m_nType) {
          (this->*handler.pFn)(event);
          break;
        }
      }
      break;
  }
}
