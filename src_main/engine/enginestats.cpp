// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "enginestats.h"

#include "clientstats.h"
#include "filesystem_engine.h"
#include "gl_matsysiface.h"
#include "limits.h"
#include "quakedef.h"
#include "sysexternal.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/vprof.h"

#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// itty bitty interface for stat time
//-----------------------------------------------------------------------------

class CStatTime : public IClientStatsTime {
 public:
  float GetTime() { return Plat_FloatTime(); }
};

CStatTime g_StatTime;

CEngineStats::CEngineStats() : m_InFrame(false) { m_bInRun = false; }

void CEngineStats::BeginRun() {
  m_bInRun = true;
  m_totalNumFrames = 0;

  // frame timing data
  m_runStartTime = Plat_FloatTime();
}

void CEngineStats::EndRun() {
  m_runEndTime = Plat_FloatTime();
  m_bInRun = false;
}

void CEngineStats::BeginFrame() {
  m_bPaused = false;
  m_InFrame = false;
}

void CEngineStats::ComputeFrameTimeStats() {
  m_StatGroup.m_StatFrameTime[ENGINE_STATS_FRAME_TIME] =
      m_flFrameTime / 1000.0f;
  m_StatGroup.m_StatFrameTime[ENGINE_STATS_FPS_VARIABILITY] =
      m_flFPSVariability / 1000.0f;
  m_StatGroup.m_StatFrameTime[ENGINE_STATS_FPS] =
      (m_flFrameTime != 0.0f) ? (1.0f / (1000.0f * m_flFrameTime)) : 0.0f;
}

void CEngineStats::EndFrame() {
  if (!m_InFrame) return;
}

//-----------------------------------------------------------------------------
// Advances the next frame for the stats...
//-----------------------------------------------------------------------------
void CEngineStats::NextFrame() {}

//-----------------------------------------------------------------------------
// Pause those stats!
//-----------------------------------------------------------------------------
void CEngineStats::PauseStats(bool bPaused) {
  if (bPaused) {
    if (m_InFrame) {
      m_bPaused = true;
      m_InFrame = false;
    }
  } else  // !bPaused
  {
    if (m_bPaused) {
      m_InFrame = true;
      m_bPaused = false;
    }
  }
}

//-----------------------------------------------------------------------------
// returns timed stats
//-----------------------------------------------------------------------------

double CEngineStats::TimedStatInFrame(EngineTimedStatId_t stat) const {
  return m_StatGroup.m_StatFrameTime[stat];
}

double CEngineStats::TotalTimedStat(EngineTimedStatId_t stat) const {
  return m_StatGroup.m_TotalStatTime[stat];
}

double CEngineStats::GetRunTime() { return m_runEndTime - m_runStartTime; }
