// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "iengine.h"

#include <cassert>
#include "cdll_engine_int.h"
#include "cdll_int.h"
#include "cl_demo.h"
#include "engine_launcher_api.h"
#include "filesystem_engine.h"
#include "gl_cvars.h"
#include "gl_matsysiface.h"
#include "host.h"
#include "host_state.h"
#include "igame.h"
#include "inputsystem/iinputsystem.h"
#include "ivideomode.h"
#include "keys.h"
#include "modes.h"
#include "profile.h"
#include "quakedef.h"
#include "server.h"
#include "sys.h"
#include "sys_dll.h"
#include "tier0/include/vprof.h"
#include "toolframework/itoolframework.h"
#include "toolframework/itoolsystem.h"
#include "vmodes.h"
#include "vprof_engine.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif

#include "tier0/include/memdbgon.h"

// Forward declarations

i32 Sys_InitGame(CreateInterfaceFn appSystemFactory, ch const *pBaseDir,
                 void *pwnd, i32 bIsDedicated);
void Sys_ShutdownGame();

// sleep time when not focus
#define NOT_FOCUS_SLEEP 50

static ConVar fps_max("fps_max", "300", 0, "Frame rate limiter");

#ifndef _RETAIL
static ConVar async_serialize("async_serialize", "0", 0,
                              "Force async reads to serialize for profiling");
#define ShouldSerializeAsync() async_serialize.GetBool()
#else
#define ShouldSerializeAsync() false
#endif

class CEngine : public IEngine {
 public:
  CEngine() {
    dll_state_ = DLL_INACTIVE;
    next_dll_state_ = DLL_INACTIVE;

    current_time_ = 0.0;
    frame_time_ = 0.0f;
    previous_time_ = 0.0;
    filtered_time_ = 0.0f;

    quitting_state_ = QUIT_NOTQUITTING;
  }

  virtual ~CEngine() {}

  bool Load(bool is_dedicated, const ch *basedir) override {
    // Activate engine
    // NOTE: We must bypass the 'next state' block here for initialization to
    // work properly.
    dll_state_ = next_dll_state_ = InEditMode() ? DLL_PAUSED : DLL_ACTIVE;

    const bool is_loaded =
        Sys_InitGame(g_AppSystemFactory, basedir, game->GetMainWindowAddress(),
                     is_dedicated);
    if (is_loaded) {
      UpdateMaterialSystemConfig();
    }

    return is_loaded;
  }

  void Unload() override {
    Sys_ShutdownGame();

    dll_state_ = DLL_INACTIVE;
    next_dll_state_ = DLL_INACTIVE;
  }

  EngineState_t GetState() const override { return dll_state_; }

  void SetNextState(EngineState_t iNextState) override {
    next_dll_state_ = iNextState;
  }

  void Frame() override {
    // yield the CPU for a little while when paused, minimized, or not the focus
    // TODO(d.rattman):  Move this to main windows message pump?
    if (!game->IsActiveApp() && !sv.IsDedicated()) {
      g_pInputSystem->SleepUntilInput(NOT_FOCUS_SLEEP);
    }

    // Get current time
    current_time_ = Plat_FloatTime();

    // Determine dt since we last checked
    f64 dt = current_time_ - previous_time_;

    // Remember old time
    previous_time_ = current_time_;

    // Accumulate current time delta into the true "frametime"
    frame_time_ += dt;

    // If the time is < 0, that means we've restarted.
    // Set the new time high enough so the engine will run a frame
    if (frame_time_ < 0.0f) return;

    // If the frametime is still too short, don't pass through
    if (!FilterTime(frame_time_)) {
      filtered_time_ += dt;
      return;
    }

    if (ShouldSerializeAsync()) {
      static ConVar *pSyncReportConVar =
          g_pCVar->FindVar("fs_report_sync_opens");
      bool bReportingSyncOpens =
          (pSyncReportConVar && pSyncReportConVar->GetInt());
      i32 reportLevel = 0;
      if (bReportingSyncOpens) {
        reportLevel = pSyncReportConVar->GetInt();
        pSyncReportConVar->SetValue(0);
      }
      g_pFileSystem->AsyncFinishAll();
      if (bReportingSyncOpens) {
        pSyncReportConVar->SetValue(reportLevel);
      }
    }

#ifdef VPROF_ENABLED
    PreUpdateProfile(filtered_time_);
#endif

    // Reset swallowed time...
    filtered_time_ = 0.0f;

#ifndef SWDS
    if (!sv.IsDedicated()) {
      ClientDLL_FrameStageNotify(FRAME_START);
    }
#endif

#ifdef VPROF_ENABLED
    PostUpdateProfile();
#endif

    {  // profile scope

      VPROF_BUDGET("CEngine::Frame", VPROF_BUDGETGROUP_OTHER_UNACCOUNTED);

      switch (dll_state_) {
        case DLL_PAUSED:    // paused, in hammer
        case DLL_INACTIVE:  // no dll
          break;

        case DLL_ACTIVE:   // engine is focused
        case DLL_CLOSE:    // closing down dll
        case DLL_RESTART:  // engine is shutting down but will restart right
                           // away Run the engine frame
          HostState_Frame(frame_time_);
          break;
      }

      // Has the state changed?
      if (next_dll_state_ != dll_state_) {
        dll_state_ = next_dll_state_;

        // Do special things if we change to particular states
        switch (dll_state_) {
          case DLL_CLOSE:
            SetQuitting(QUIT_TODESKTOP);
            break;
          case DLL_RESTART:
            SetQuitting(QUIT_RESTART);
            break;
        }
      }

    }  // profile scope

    // Reset for next frame
    frame_time_ = 0.0f;
  }

  f32 GetFrameTime() const override { return frame_time_; }
  f32 GetCurTime() const override { return current_time_; }

  i32 GetQuitting() const override { return quitting_state_; }
  void SetQuitting(i32 quittype) override { quitting_state_ = quittype; }

 private:
  bool FilterTime(f32 dt) {
    // Dedicated's tic_rate regulates server frame rate.  Don't apply fps filter
    // here. Only do this restriction on the client. Prevents clients from
    // accomplishing certain hacks by pausing their client for a period of time.
    if (!sv.IsDedicated() && !CanCheat() && fps_max.GetFloat() < 30) {
      // Don't do anything if fps_max=0 (which means it's unlimited).
      if (fps_max.GetFloat() != 0.0f) {
        Warning(
            "sv_cheats is 0 and fps_max is being limited to a minimum of 30 "
            "(or "
            "set to 0).\n");
        fps_max.SetValue(30.0f);
      }
    }

    f32 fps = fps_max.GetFloat();
    if (fps > 0.0f) {
      // Limit fps to withing tolerable range
      //		fps = std::max( MIN_FPS, fps ); // red herring - since
      // we're
      // only
      // checking if dt < 1/fps, clamping against MIN_FPS has no effect
      fps = std::min(MAX_FPS, fps);

      f32 minframetime = 1.0 / fps;

      if (
#if !defined(SWDS)
          !demoplayer->IsPlayingTimeDemo() &&
#endif
          dt < minframetime) {
        // framerate is too high
        return false;
      }
    }

    return true;
  }

  i32 quitting_state_;

  EngineState_t dll_state_;
  EngineState_t next_dll_state_;

  f64 current_time_;
  f32 frame_time_;
  f64 previous_time_;
  f32 filtered_time_;
};

static CEngine g_Engine;
IEngine *eng = &g_Engine;
