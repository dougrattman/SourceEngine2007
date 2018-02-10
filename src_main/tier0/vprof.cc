// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Real-Time Hierarchical Profiling

#include "pch_tier0.h"

#include "tier0/include/vprof.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <vector>

#include "tier0/include/compiler_specific_macroses.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/valve_off.h"

#ifdef OS_WIN
MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4073)
#pragma init_seg(lib)
MSVC_END_WARNING_OVERRIDE_SCOPE()
#endif

#include "tier0/include/valve_on.h"

#include "tier0/include/memdbgon.h"

// NOTE: Explicitly and intentionally using STL in here to not generate any
// cyclical dependencies between the low-level debug library and the higher
// level data structures (toml 01-27-03)
using namespace std;

#ifdef VPROF_ENABLED

//-----------------------------------------------------------------------------

CVProfile g_VProfCurrentProfile;

i32 CVProfNode::s_iCurrentUniqueNodeID = 0;

CVProfNode *CVProfNode::GetSubNode(const ch *pszName, i32 detailLevel,
                                   const ch *pBudgetGroupName,
                                   i32 budgetFlags) {
  // Try to find this sub node
  CVProfNode *child = m_pChild;
  while (child) {
    if (child->m_pszName == pszName) {
      return child;
    }
    child = child->m_pSibling;
  }

  // We didn't find it, so add it
  CVProfNode *node =
      new CVProfNode(pszName, detailLevel, this, pBudgetGroupName, budgetFlags);
  node->m_pSibling = m_pChild;
  m_pChild = node;
  return node;
}

CVProfNode *CVProfNode::GetSubNode(const ch *pszName, i32 detailLevel,
                                   const ch *pBudgetGroupName) {
  return GetSubNode(pszName, detailLevel, pBudgetGroupName, BUDGETFLAG_OTHER);
}

//-------------------------------------

void CVProfNode::EnterScope() {
  m_nCurFrameCalls++;
  if (m_nRecursions++ == 0) {
    m_Timer.Start();

#ifdef VPROF_VTUNE_GROUP
    g_VProfCurrentProfile.PushGroup(m_BudgetGroupID);
#endif
  }
}

//-------------------------------------

bool CVProfNode::ExitScope() {
  if (--m_nRecursions == 0 && m_nCurFrameCalls != 0) {
    m_Timer.End();
    m_CurFrameTime += m_Timer.GetDuration();

#ifdef VPROF_VTUNE_GROUP
    g_VProfCurrentProfile.PopGroup();
#endif
  }
  return (m_nRecursions == 0);
}

//-------------------------------------

void CVProfNode::Pause() {
  if (m_nRecursions > 0) {
    m_Timer.End();
    m_CurFrameTime += m_Timer.GetDuration();
  }
  if (m_pChild) {
    m_pChild->Pause();
  }
  if (m_pSibling) {
    m_pSibling->Pause();
  }
}

//-------------------------------------

void CVProfNode::Resume() {
  if (m_nRecursions > 0) {
    m_Timer.Start();
  }
  if (m_pChild) {
    m_pChild->Resume();
  }
  if (m_pSibling) {
    m_pSibling->Resume();
  }
}

//-------------------------------------

void CVProfNode::Reset() {
  m_nPrevFrameCalls = 0;
  m_PrevFrameTime.Init();

  m_nCurFrameCalls = 0;
  m_CurFrameTime.Init();

  m_nTotalCalls = 0;
  m_TotalTime.Init();

  m_PeakTime.Init();

  if (m_pChild) {
    m_pChild->Reset();
  }
  if (m_pSibling) {
    m_pSibling->Reset();
  }
}

//-------------------------------------

void CVProfNode::MarkFrame() {
  m_nPrevFrameCalls = m_nCurFrameCalls;
  m_PrevFrameTime = m_CurFrameTime;
  m_nTotalCalls += m_nCurFrameCalls;
  m_TotalTime += m_CurFrameTime;

  if (m_PeakTime.IsLessThan(m_CurFrameTime)) {
    m_PeakTime = m_CurFrameTime;
  }

  m_CurFrameTime.Init();
  m_nCurFrameCalls = 0;

  if (m_pChild) {
    m_pChild->MarkFrame();
  }
  if (m_pSibling) {
    m_pSibling->MarkFrame();
  }
}

//-------------------------------------

void CVProfNode::ResetPeak() {
  m_PeakTime.Init();

  if (m_pChild) {
    m_pChild->ResetPeak();
  }
  if (m_pSibling) {
    m_pSibling->ResetPeak();
  }
}

void CVProfNode::SetCurFrameTime(unsigned long milliseconds) {
  m_CurFrameTime.Init((f32)milliseconds);
}
#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var
// in  our  container)
//-----------------------------------------------------------------------------
void CVProfNode::Validate(CValidator &validator, ch *pchName) {
  validator.Push(_T("CVProfNode"), this, pchName);

  if (m_pSibling) m_pSibling->Validate(validator, _T("m_pSibling"));
  if (m_pChild) m_pChild->Validate(validator, _T("m_pChild"));

  validator.Pop();
}
#endif  // DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------

struct TimeSums_t {
  const ch *pszProfileScope;
  u32 calls;
  f64 time;
  f64 timeLessChildren;
  f64 peak;
};

static bool TimeCompare(const TimeSums_t &lhs, const TimeSums_t &rhs) {
  return (lhs.time > rhs.time);
}

static bool TimeLessChildrenCompare(const TimeSums_t &lhs,
                                    const TimeSums_t &rhs) {
  return (lhs.timeLessChildren > rhs.timeLessChildren);
}

static bool PeakCompare(const TimeSums_t &lhs, const TimeSums_t &rhs) {
  return (lhs.peak > rhs.peak);
}

static bool AverageTimeCompare(const TimeSums_t &lhs, const TimeSums_t &rhs) {
  f64 avgLhs = (lhs.calls) ? lhs.time / (f64)lhs.calls : 0.0;
  f64 avgRhs = (rhs.calls) ? rhs.time / (f64)rhs.calls : 0.0;
  return (avgLhs > avgRhs);
}

static bool AverageTimeLessChildrenCompare(const TimeSums_t &lhs,
                                           const TimeSums_t &rhs) {
  f64 avgLhs = (lhs.calls) ? lhs.timeLessChildren / (f64)lhs.calls : 0.0;
  f64 avgRhs = (rhs.calls) ? rhs.timeLessChildren / (f64)rhs.calls : 0.0;
  return (avgLhs > avgRhs);
}
static bool PeakOverAverageCompare(const TimeSums_t &lhs,
                                   const TimeSums_t &rhs) {
  f64 avgLhs = (lhs.calls) ? lhs.time / (f64)lhs.calls : 0.0;
  f64 avgRhs = (rhs.calls) ? rhs.time / (f64)rhs.calls : 0.0;

  f64 lhsPoA = (avgLhs != 0) ? lhs.peak / avgLhs : 0.0;
  f64 rhsPoA = (avgRhs != 0) ? rhs.peak / avgRhs : 0.0;

  return (lhsPoA > rhsPoA);
}

map<CVProfNode *, f64> g_TimesLessChildren;
i32 g_TotalFrames;
map<const ch *, usize> g_TimeSumsMap;
vector<TimeSums_t> g_TimeSums;
CVProfNode *g_pStartNode;
const ch *g_pszSumNode;

//-------------------------------------

void CVProfile::SumTimes(CVProfNode *pNode, i32 budgetGroupID) {
  if (!pNode) return;  // this generally only happens on a failed FindNode()

  bool bSetStartNode;
  if (!g_pStartNode && strcmp(pNode->GetName(), g_pszSumNode) == 0) {
    g_pStartNode = pNode;
    bSetStartNode = true;
  } else
    bSetStartNode = false;

  if (GetRoot() != pNode) {
    if (g_pStartNode && pNode->GetTotalCalls() > 0 &&
        (budgetGroupID == -1 || pNode->GetBudgetGroupID() == budgetGroupID)) {
      f64 timeLessChildren = pNode->GetTotalTimeLessChildren();

      g_TimesLessChildren.insert(make_pair(pNode, timeLessChildren));

      const auto iter =
          g_TimeSumsMap.find(pNode->GetName());  // intenionally using
                                                 // address of string rather
                                                 // than string compare (toml
                                                 // 01-27-03)
      if (iter == g_TimeSumsMap.end()) {
        TimeSums_t timeSums = {pNode->GetName(), pNode->GetTotalCalls(),
                               pNode->GetTotalTime(), timeLessChildren,
                               pNode->GetPeakTime()};
        g_TimeSumsMap.insert(make_pair(pNode->GetName(), g_TimeSums.size()));
        g_TimeSums.push_back(timeSums);
      } else {
        TimeSums_t &timeSums = g_TimeSums[iter->second];
        timeSums.calls += pNode->GetTotalCalls();
        timeSums.time += pNode->GetTotalTime();
        timeSums.timeLessChildren += timeLessChildren;
        if (pNode->GetPeakTime() > timeSums.peak)
          timeSums.peak = pNode->GetPeakTime();
      }
    }

    if ((!g_pStartNode || pNode != g_pStartNode) && pNode->GetSibling()) {
      SumTimes(pNode->GetSibling(), budgetGroupID);
    }
  }

  if (pNode->GetChild()) {
    SumTimes(pNode->GetChild(), budgetGroupID);
  }

  if (bSetStartNode) g_pStartNode = nullptr;
}

//-------------------------------------

CVProfNode *CVProfile::FindNode(CVProfNode *pStartNode, const ch *pszNode) {
  if (strcmp(pStartNode->GetName(), pszNode) != 0) {
    CVProfNode *pFoundNode = nullptr;
    if (pStartNode->GetSibling()) {
      pFoundNode = FindNode(pStartNode->GetSibling(), pszNode);
    }

    if (!pFoundNode && pStartNode->GetChild()) {
      pFoundNode = FindNode(pStartNode->GetChild(), pszNode);
    }

    return pFoundNode;
  }
  return pStartNode;
}

//-------------------------------------

void CVProfile::SumTimes(const ch *pszStartNode, i32 budgetGroupID) {
  if (GetRoot()->GetChild()) {
    if (pszStartNode == nullptr)
      g_pStartNode = GetRoot();
    else
      g_pStartNode = nullptr;

    g_pszSumNode = pszStartNode;
    SumTimes(GetRoot(), budgetGroupID);
    g_pStartNode = nullptr;
  }
}

//-------------------------------------

void CVProfile::DumpNodes(CVProfNode *pNode, i32 indent,
                          bool bAverageAndCountOnly) {
  if (!pNode) return;  // this generally only happens on a failed FindNode()

  bool fIsRoot = (pNode == GetRoot());

  if (fIsRoot || pNode == g_pStartNode) {
    if (bAverageAndCountOnly) {
      Msg(" Avg Time/Frame (ms)\n");
      Msg("[ func+child   func ]     Count\n");
      Msg("  ---------- ------      ------\n");
    } else {
      Msg("       Sum (ms)         Avg Time/Frame (ms)     Avg Time/Call "
          "(ms)\n");
      Msg("[ func+child   func ]  [ func+child   func ]  [ func+child   "
          "func ]  Count   Peak\n");
      Msg("  ---------- ------      ---------- ------      ---------- "
          "------   ------ ------\n");
    }
  }

  if (!fIsRoot) {
    auto iterTimeLessChildren = g_TimesLessChildren.find(pNode);
    Assert(iterTimeLessChildren != g_TimesLessChildren.end());

    if (bAverageAndCountOnly) {
      Msg("  %10.3f %6.2f      %6d",
          (pNode->GetTotalCalls() > 0)
              ? pNode->GetTotalTime() / (f64)NumFramesSampled()
              : 0,
          (pNode->GetTotalCalls() > 0)
              ? iterTimeLessChildren->second / (f64)NumFramesSampled()
              : 0,
          pNode->GetTotalCalls());
    } else {
      Msg("  %10.3f %6.2f      %10.3f %6.2f      %10.3f %6.2f   %6d %6.2f",
          pNode->GetTotalTime(), iterTimeLessChildren->second,
          (pNode->GetTotalCalls() > 0)
              ? pNode->GetTotalTime() / (f64)NumFramesSampled()
              : 0,
          (pNode->GetTotalCalls() > 0)
              ? iterTimeLessChildren->second / (f64)NumFramesSampled()
              : 0,
          (pNode->GetTotalCalls() > 0)
              ? pNode->GetTotalTime() / (f64)pNode->GetTotalCalls()
              : 0,
          (pNode->GetTotalCalls() > 0)
              ? iterTimeLessChildren->second / (f64)pNode->GetTotalCalls()
              : 0,
          pNode->GetTotalCalls(), pNode->GetPeakTime());
    }

    Msg("  ");
    for (i32 i = 1; i < indent; i++) {
      Msg("|  ");
    }

    Msg("%s\n", pNode->GetName());
  }

  if (pNode->GetChild()) {
    DumpNodes(pNode->GetChild(), indent + 1, bAverageAndCountOnly);
  }

  if (!(fIsRoot || pNode == g_pStartNode) && pNode->GetSibling()) {
    DumpNodes(pNode->GetSibling(), indent, bAverageAndCountOnly);
  }
}

//-------------------------------------
static void DumpSorted(const ch *pszHeading, f64 totalTime,
                       bool (*pfnSort)(const TimeSums_t &, const TimeSums_t &),
                       i32 maxLen = 999999) {
  u32 i;
  vector<TimeSums_t> sortedSums;
  sortedSums = g_TimeSums;
  sort(sortedSums.begin(), sortedSums.end(), pfnSort);

  Msg("%s\n", pszHeading);
  Msg("  Scope                                                      Calls "
      "Calls/Frame  Time+Child    Pct        Time    Pct   Avg/Frame    "
      "Avg/Call Avg-NoChild        Peak\n");
  Msg("  ---------------------------------------------------- ----------- "
      "----------- ----------- ------ ----------- ------ ----------- "
      "----------- ----------- -----------\n");
  for (i = 0; i < sortedSums.size() && i < (u32)maxLen; i++) {
    f64 avg = (sortedSums[i].calls)
                  ? sortedSums[i].time / (f64)sortedSums[i].calls
                  : 0.0;
    f64 avgLessChildren =
        (sortedSums[i].calls)
            ? sortedSums[i].timeLessChildren / (f64)sortedSums[i].calls
            : 0.0;

    Msg("  "
        "%52.52s%12d%12.3f%12.3f%6.2f%%%12.3f%6.2f%%%12.3f%12.3f%12.3f%12.3f\n",
        sortedSums[i].pszProfileScope, sortedSums[i].calls,
        (f32)sortedSums[i].calls / (f32)g_TotalFrames, sortedSums[i].time,
        (sortedSums[i].time / totalTime) * 100.0,
        sortedSums[i].timeLessChildren,
        (sortedSums[i].timeLessChildren / totalTime) * 100.0,
        sortedSums[i].time / (f32)g_TotalFrames, avg, avgLessChildren,
        sortedSums[i].peak);
  }
}

//-------------------------------------

void CVProfile::OutputReport(i32 type, const ch *pszStartNode,
                             i32 budgetGroupID) {
  Msg("******** BEGIN VPROF REPORT ********\n");
#ifdef COMPILER_MSVC
#if (_MSC_VER < 1300)
  Msg(_T("  (note: this report exceeds the output capacity of MSVC debug ")
      _T("window. Use console window or console log.) \n"));
#endif
#endif

  g_TotalFrames = max(NumFramesSampled() - 1, 1);

  if (NumFramesSampled() == 0 || GetTotalTimeSampled() == 0)
    Msg("No samples\n");
  else {
    if (type & VPRT_SUMMARY) {
      Msg("-- Summary --\n");
      Msg("%d frames sampled for %.2f seconds\n", g_TotalFrames,
          GetTotalTimeSampled() / 1000.0);
      Msg("Average %.2f fps, %.2f ms per frame\n",
          1000.0 / (GetTotalTimeSampled() / g_TotalFrames),
          GetTotalTimeSampled() / g_TotalFrames);
      Msg("Peak %.2f ms frame\n", GetPeakFrameTime());

      f64 timeAccountedFor =
          100.0 - (m_Root.GetTotalTimeLessChildren() / m_Root.GetTotalTime());
      Msg("%.0f pct of time accounted for\n", min(100.0, timeAccountedFor));
      Msg("\n");
    }

    if (pszStartNode == nullptr) {
      pszStartNode = GetRoot()->GetName();
    }

    SumTimes(pszStartNode, budgetGroupID);

    // Dump the hierarchy
    if (type & VPRT_HIERARCHY) {
      Msg("-- Hierarchical Call Graph --\n");
      if (pszStartNode == nullptr)
        g_pStartNode = nullptr;
      else
        g_pStartNode = FindNode(GetRoot(), pszStartNode);

      DumpNodes((!g_pStartNode) ? GetRoot() : g_pStartNode, 0, false);
      Msg("\n");
    }

    if (type & VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY) {
      Msg("-- Hierarchical Call Graph --\n");
      if (pszStartNode == nullptr)
        g_pStartNode = nullptr;
      else
        g_pStartNode = FindNode(GetRoot(), pszStartNode);

      DumpNodes((!g_pStartNode) ? GetRoot() : g_pStartNode, 0, true);
      Msg("\n");
    }

    i32 maxLen = (type & VPRT_LIST_TOP_ITEMS_ONLY) ? 25 : 999999;

    if (type & VPRT_LIST_BY_TIME) {
      DumpSorted("-- Profile scopes sorted by time (including children) --",
                 GetTotalTimeSampled(), TimeCompare, maxLen);
      Msg("\n");
    }
    if (type & VPRT_LIST_BY_TIME_LESS_CHILDREN) {
      DumpSorted("-- Profile scopes sorted by time (without children) --",
                 GetTotalTimeSampled(), TimeLessChildrenCompare, maxLen);
      Msg("\n");
    }
    if (type & VPRT_LIST_BY_AVG_TIME) {
      DumpSorted(
          "-- Profile scopes sorted by average time (including children) "
          "--",
          GetTotalTimeSampled(), AverageTimeCompare, maxLen);
      Msg("\n");
    }
    if (type & VPRT_LIST_BY_AVG_TIME_LESS_CHILDREN) {
      DumpSorted(
          "-- Profile scopes sorted by average time (without children) --",
          GetTotalTimeSampled(), AverageTimeLessChildrenCompare, maxLen);
      Msg("\n");
    }
    if (type & VPRT_LIST_BY_PEAK_TIME) {
      DumpSorted("-- Profile scopes sorted by peak --", GetTotalTimeSampled(),
                 PeakCompare, maxLen);
      Msg("\n");
    }
    if (type & VPRT_LIST_BY_PEAK_OVER_AVERAGE) {
      DumpSorted(
          "-- Profile scopes sorted by peak over average (including "
          "children) --",
          GetTotalTimeSampled(), PeakOverAverageCompare, maxLen);
      Msg("\n");
    }

    // TODO: Functions by time less children
    // TODO: Functions by time averages
    // TODO: Functions by peak
    // TODO: Peak deviation from average
    g_TimesLessChildren.clear();
    g_TimeSumsMap.clear();
    g_TimeSums.clear();
  }
  Msg("******** END VPROF REPORT ********\n");
}

//=============================================================================

CVProfile::CVProfile()
    : m_Root("Root", 0, nullptr, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, 0),
      m_pCurNode(&m_Root),
      m_nFrames(0),
      m_enabled(0),
      m_pausedEnabledDepth(0),
      m_fAtRoot(true) {
#ifdef VPROF_VTUNE_GROUP
  m_GroupIDStackDepth = 1;
  m_GroupIDStack[0] = 0;  // VPROF_BUDGETGROUP_OTHER_UNACCOUNTED
#endif

  // Go ahead and allocate 32 slots for budget group names
  MEM_ALLOC_CREDIT();
  m_pBudgetGroups = new CVProfile::CBudgetGroup[32];
  m_nBudgetGroupNames = 0;
  m_nBudgetGroupNamesAllocated = 32;

  // Add these here so that they will always be in the same order.
  // VPROF_BUDGETGROUP_OTHER_UNACCOUNTED has to be FIRST!!!!
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OTHER_UNACCOUNTED,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_WORLD_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_GAME,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_PLAYER,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_NPCS,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_SERVER_ANIM,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_CLIENT_ANIMATION,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_PHYSICS,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_STATICPROP_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_MODEL_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_LIGHTCACHE,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_SHADOW_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_DETAILPROP_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_PARTICLE_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_ROPES, BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_DLIGHT_RENDERING,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OTHER_NETWORKING,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OTHER_SOUND,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OTHER_VGUI,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OTHER_FILESYSTEM,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_SERVER);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_PREDICTION,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_INTERPOLATION,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_SWAP_BUFFERS,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OCCLUSION,
                                 BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_OVERLAYS, BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_TOOLS,
                                 BUDGETFLAG_OTHER | BUDGETFLAG_CLIENT);
  BudgetGroupNameToBudgetGroupID(VPROF_BUDGETGROUP_TEXTURE_CACHE,
                                 BUDGETFLAG_CLIENT);
  //	BudgetGroupNameToBudgetGroupID( VPROF_BUDGETGROUP_DISP_HULLTRACES );

  m_bPMEInit = false;
  m_bPMEEnabled = false;
}

CVProfile::~CVProfile() { Term(); }

void CVProfile::FreeNodes_R(CVProfNode *pNode) {
  CVProfNode *pNext;
  for (CVProfNode *pChild = pNode->GetChild(); pChild; pChild = pNext) {
    pNext = pChild->GetSibling();
    FreeNodes_R(pChild);
  }

  if (pNode == GetRoot()) {
    pNode->m_pChild = nullptr;
  } else {
    delete pNode;
  }
}

void CVProfile::Term() {
  i32 i;
  for (i = 0; i < m_nBudgetGroupNames; i++) {
    delete[] m_pBudgetGroups[i].m_pName;
  }
  delete[] m_pBudgetGroups;
  m_nBudgetGroupNames = m_nBudgetGroupNamesAllocated = 0;
  m_pBudgetGroups = nullptr;

  i32 n;
  for (n = 0; n < m_NumCounters; n++) {
    delete[] m_CounterNames[n];
    m_CounterNames[n] = nullptr;
  }
  m_NumCounters = 0;

  // Free the nodes.
  FreeNodes_R(GetRoot());
}

#define COLORMIN 160
#define COLORMAX 255

static i32 g_ColorLookup[4] = {
    COLORMIN,
    COLORMAX,
    COLORMIN + (COLORMAX - COLORMIN) / 3,
    COLORMIN + ((COLORMAX - COLORMIN) * 2) / 3,
};

#define GET_BIT(val, bitnum) (((val) >> (bitnum)) & 0x1)

void CVProfile::GetBudgetGroupColor(i32 budgetGroupID, i32 &r, i32 &g, i32 &b,
                                    i32 &a) {
  budgetGroupID = budgetGroupID % (1 << 6);

  i32 index;
  index = GET_BIT(budgetGroupID, 0) | (GET_BIT(budgetGroupID, 5) << 1);
  r = g_ColorLookup[index];
  index = GET_BIT(budgetGroupID, 1) | (GET_BIT(budgetGroupID, 4) << 1);
  g = g_ColorLookup[index];
  index = GET_BIT(budgetGroupID, 2) | (GET_BIT(budgetGroupID, 3) << 1);
  b = g_ColorLookup[index];
  a = 255;
}

// return -1 if it doesn't exist.
i32 CVProfile::FindBudgetGroupName(const ch *pBudgetGroupName) {
  i32 i;
  for (i = 0; i < m_nBudgetGroupNames; i++) {
    if (stricmp(pBudgetGroupName, m_pBudgetGroups[i].m_pName) == 0) {
      return i;
    }
  }
  return -1;
}

i32 CVProfile::AddBudgetGroupName(const ch *pBudgetGroupName, i32 budgetFlags) {
  MEM_ALLOC_CREDIT();
  ch *pNewString = new ch[strlen(pBudgetGroupName) + 1];
  strcpy(pNewString, pBudgetGroupName);
  if (m_nBudgetGroupNames + 1 > m_nBudgetGroupNamesAllocated) {
    m_nBudgetGroupNamesAllocated *= 2;
    m_nBudgetGroupNamesAllocated =
        max(m_nBudgetGroupNames + 6, m_nBudgetGroupNamesAllocated);

    CBudgetGroup *pNew = new CBudgetGroup[m_nBudgetGroupNamesAllocated];
    for (i32 i = 0; i < m_nBudgetGroupNames; i++) pNew[i] = m_pBudgetGroups[i];

    delete[] m_pBudgetGroups;
    m_pBudgetGroups = pNew;
  }

  m_pBudgetGroups[m_nBudgetGroupNames].m_pName = pNewString;
  m_pBudgetGroups[m_nBudgetGroupNames].m_BudgetFlags = budgetFlags;
  m_nBudgetGroupNames++;

  if (m_pNumBudgetGroupsChangedCallBack) {
    (*m_pNumBudgetGroupsChangedCallBack)();
  }

  return m_nBudgetGroupNames - 1;
}

i32 CVProfile::BudgetGroupNameToBudgetGroupID(const ch *pBudgetGroupName,
                                              i32 budgetFlagsToORIn) {
  i32 budgetGroupID = FindBudgetGroupName(pBudgetGroupName);
  if (budgetGroupID == -1) {
    budgetGroupID = AddBudgetGroupName(pBudgetGroupName, budgetFlagsToORIn);
  } else {
    m_pBudgetGroups[budgetGroupID].m_BudgetFlags |= budgetFlagsToORIn;
  }

  return budgetGroupID;
}

i32 CVProfile::BudgetGroupNameToBudgetGroupID(const ch *pBudgetGroupName) {
  return BudgetGroupNameToBudgetGroupID(pBudgetGroupName, BUDGETFLAG_OTHER);
}

i32 CVProfile::GetNumBudgetGroups() { return m_nBudgetGroupNames; }

void CVProfile::RegisterNumBudgetGroupsChangedCallBack(
    void (*pCallBack)(void)) {
  m_pNumBudgetGroupsChangedCallBack = pCallBack;
}

void CVProfile::HideBudgetGroup(i32 budgetGroupID, bool bHide) {
  if (budgetGroupID != -1) {
    if (bHide)
      m_pBudgetGroups[budgetGroupID].m_BudgetFlags |= BUDGETFLAG_HIDDEN;
    else
      m_pBudgetGroups[budgetGroupID].m_BudgetFlags &= ~BUDGETFLAG_HIDDEN;
  }
}

i32 *CVProfile::FindOrCreateCounter(const ch *pName,
                                    CounterGroup_t eCounterGroup) {
  Assert(m_NumCounters + 1 < MAXCOUNTERS);
  if (m_NumCounters + 1 >= MAXCOUNTERS || !ThreadInMainThread()) {
    static i32 dummy;
    return &dummy;
  }
  i32 i;
  for (i = 0; i < m_NumCounters; i++) {
    if (stricmp(m_CounterNames[i], pName) == 0) {
      // found it!
      return &m_Counters[i];
    }
  }

  // NOTE: These get freed in ~CVProfile.
  MEM_ALLOC_CREDIT();
  ch *pNewName = new ch[strlen(pName) + 1];
  strcpy(pNewName, pName);
  m_Counters[m_NumCounters] = 0;
  m_CounterGroups[m_NumCounters] = (ch)eCounterGroup;
  m_CounterNames[m_NumCounters++] = pNewName;
  return &m_Counters[m_NumCounters - 1];
}

void CVProfile::ResetCounters(CounterGroup_t eCounterGroup) {
  i32 i;
  for (i = 0; i < m_NumCounters; i++) {
    if (m_CounterGroups[i] == eCounterGroup) m_Counters[i] = 0;
  }
}

i32 CVProfile::GetNumCounters() const { return m_NumCounters; }

const ch *CVProfile::GetCounterName(i32 index) const {
  Assert(index >= 0 && index < m_NumCounters);
  return m_CounterNames[index];
}

i32 CVProfile::GetCounterValue(i32 index) const {
  Assert(index >= 0 && index < m_NumCounters);
  return m_Counters[index];
}

const ch *CVProfile::GetCounterNameAndValue(i32 index, i32 &val) const {
  Assert(index >= 0 && index < m_NumCounters);
  val = m_Counters[index];
  return m_CounterNames[index];
}

CounterGroup_t CVProfile::GetCounterGroup(i32 index) const {
  Assert(index >= 0 && index < m_NumCounters);
  return (CounterGroup_t)m_CounterGroups[index];
}

#ifdef DBGFLAG_VALIDATE

#ifdef ARCH_CPU_X86_64
#error The below is presumably broken on 64 bit
#endif  // ARCH_CPU_X86_64

const i32 k_cSTLMapAllocOffset = 4;
#define GET_INTERNAL_MAP_ALLOC_PTR(pMap) \
  (*((void **)(((byte *)(pMap)) + k_cSTLMapAllocOffset)))
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var
// in  our  container)
//-----------------------------------------------------------------------------
void CVProfile::Validate(CValidator &validator, ch *pchName) {
  validator.Push(_T("CVProfile"), this, pchName);

  m_Root.Validate(validator, _T("m_Root"));

  for (i32 iBudgetGroup = 0; iBudgetGroup < m_nBudgetGroupNames; iBudgetGroup++)
    validator.ClaimMemory(m_pBudgetGroups[iBudgetGroup].m_pName);

  validator.ClaimMemory(m_pBudgetGroups);

  // The std template map class allocates memory internally, but offer no way to
  // get access to their pointer.  Since this is for debug purposes only and the
  // std template classes don't change, just look at the well-known offset. This
  // is arguably sick and wrong, kind of like marrying a squirrel.
  validator.ClaimMemory(GET_INTERNAL_MAP_ALLOC_PTR(&g_TimesLessChildren));
  validator.ClaimMemory(GET_INTERNAL_MAP_ALLOC_PTR(&g_TimeSumsMap));

  validator.Pop();
}
#endif  // DBGFLAG_VALIDATE

#endif
