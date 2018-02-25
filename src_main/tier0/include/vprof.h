// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Real-Time Hierarchical Profiling

#ifndef SOURCE_TIER0_INCLUDE_VPROF_H_
#define SOURCE_TIER0_INCLUDE_VPROF_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "tier0/include/dbg.h"
#include "tier0/include/fasttimer.h"
#include "tier0/include/threadtools.h"

#define VPROF_ENABLED

// Enable this to get detailed nodes beneath budget
// #define VPROF_LEVEL 1

// Profiling instrumentation macros

#define MAXCOUNTERS 256

#ifdef VPROF_ENABLED

#define VPROF_VTUNE_GROUP

#define VPROF(name) \
  VPROF_(name, 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0)
#define VPROF_ASSERT_ACCOUNTED(name) \
  VPROF_(name, 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, true, 0)
#define VPROF_(name, detail, group, bAssertAccounted, budgetFlags) \
  VPROF_##detail(name, group, bAssertAccounted, budgetFlags)

#define VPROF_BUDGET(name, group) \
  VPROF_BUDGET_FLAGS(name, group, BUDGETFLAG_OTHER)
#define VPROF_BUDGET_FLAGS(name, group, flags) \
  VPROF_(name, 0, group, false, flags)

#define VPROF_SCOPE_BEGIN(tag) \
  do {                         \
  VPROF(tag)
#define VPROF_SCOPE_END() \
  }                       \
  while (0)

#define VPROF_ONLY(expression) expression

#define VPROF_ENTER_SCOPE(name)     \
  g_VProfCurrentProfile.EnterScope( \
      name, 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0)
#define VPROF_EXIT_SCOPE() g_VProfCurrentProfile.ExitScope()

#define VPROF_BUDGET_GROUP_ID_UNACCOUNTED 0

// Budgetgroup flags. These are used with VPROF_BUDGET_FLAGS.
// These control which budget panels the groups show up in.
// If a budget group uses VPROF_BUDGET, it gets the default
// which is BUDGETFLAG_OTHER.
#define BUDGETFLAG_CLIENT (1 << 0)  // Shows up in the client panel.
#define BUDGETFLAG_SERVER (1 << 1)  // Shows up in the server panel.
#define BUDGETFLAG_OTHER \
  (1 << 2)  // Unclassified (the client shows these but the dedicated server
// doesn't).
#define BUDGETFLAG_HIDDEN (1 << 15)
#define BUDGETFLAG_ALL 0xFFFF

// NOTE: You can use strings instead of these defines. They are defined here
// and added in vprof.cpp so that they are always in the same order.
#define VPROF_BUDGETGROUP_OTHER_UNACCOUNTED "Unaccounted"
#define VPROF_BUDGETGROUP_WORLD_RENDERING "World Rendering"
#define VPROF_BUDGETGROUP_DISPLACEMENT_RENDERING "Displacement_Rendering"
#define VPROF_BUDGETGROUP_GAME "Game"
#define VPROF_BUDGETGROUP_NPCS "NPCs"
#define VPROF_BUDGETGROUP_SERVER_ANIM "Server Animation"
#define VPROF_BUDGETGROUP_PHYSICS "Physics"
#define VPROF_BUDGETGROUP_STATICPROP_RENDERING "Static_Prop_Rendering"
#define VPROF_BUDGETGROUP_MODEL_RENDERING "Other_Model_Rendering"
#define VPROF_BUDGETGROUP_BRUSHMODEL_RENDERING "Brush_Model_Rendering"
#define VPROF_BUDGETGROUP_SHADOW_RENDERING "Shadow_Rendering"
#define VPROF_BUDGETGROUP_DETAILPROP_RENDERING "Detail_Prop_Rendering"
#define VPROF_BUDGETGROUP_PARTICLE_RENDERING "Particle/Effect_Rendering"
#define VPROF_BUDGETGROUP_ROPES "Ropes"
#define VPROF_BUDGETGROUP_DLIGHT_RENDERING "Dynamic_Light_Rendering"
#define VPROF_BUDGETGROUP_OTHER_NETWORKING "Networking"
#define VPROF_BUDGETGROUP_CLIENT_ANIMATION "Client_Animation"
#define VPROF_BUDGETGROUP_OTHER_SOUND "Sound"
#define VPROF_BUDGETGROUP_OTHER_VGUI "VGUI"
#define VPROF_BUDGETGROUP_OTHER_FILESYSTEM "FileSystem"
#define VPROF_BUDGETGROUP_PREDICTION "Prediction"
#define VPROF_BUDGETGROUP_INTERPOLATION "Interpolation"
#define VPROF_BUDGETGROUP_SWAP_BUFFERS "Swap_Buffers"
#define VPROF_BUDGETGROUP_PLAYER "Player"
#define VPROF_BUDGETGROUP_OCCLUSION "Occlusion"
#define VPROF_BUDGETGROUP_OVERLAYS "Overlays"
#define VPROF_BUDGETGROUP_TOOLS "Tools"
#define VPROF_BUDGETGROUP_LIGHTCACHE "Light_Cache"
#define VPROF_BUDGETGROUP_DISP_HULLTRACES "Displacement_Hull_Traces"
#define VPROF_BUDGETGROUP_TEXTURE_CACHE "Texture_Cache"
#define VPROF_BUDGETGROUP_PARTICLE_SIMULATION "Particle Simulation"
#define VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING "Flashlight Shadows"
// think functions, tempents, etc.
#define VPROF_BUDGETGROUP_CLIENT_SIM "Client Simulation"
#define VPROF_BUDGETGROUP_STEAM "Steam"

#ifndef VPROF_LEVEL
#define VPROF_LEVEL 0
#endif

#define VPROF_0(name, group, assertAccounted, budgetFlags) \
  CVProfScope VProf_(name, 0, group, assertAccounted, budgetFlags);

#if VPROF_LEVEL > 0
#define VPROF_1(name, group, assertAccounted, budgetFlags) \
  CVProfScope VProf_(name, 1, group, assertAccounted, budgetFlags);
#else
#define VPROF_1(name, group, assertAccounted, budgetFlags) ((void)0)
#endif

#if VPROF_LEVEL > 1
#define VPROF_2(name, group, assertAccounted, budgetFlags) \
  CVProfScope VProf_(name, 2, group, assertAccounted, budgetFlags);
#else
#define VPROF_2(name, group, assertAccounted, budgetFlags) ((void)0)
#endif

#if VPROF_LEVEL > 2
#define VPROF_3(name, group, assertAccounted, budgetFlags) \
  CVProfScope VProf_(name, 3, group, assertAccounted, budgetFlags);
#else
#define VPROF_3(name, group, assertAccounted, budgetFlags) ((void)0)
#endif

#if VPROF_LEVEL > 3
#define VPROF_4(name, group, assertAccounted, budgetFlags) \
  CVProfScope VProf_(name, 4, group, assertAccounted, budgetFlags);
#else
#define VPROF_4(name, group, assertAccounted, budgetFlags) ((void)0)
#endif

#define VPROF_INCREMENT_COUNTER(name, amount) \
  do {                                        \
    static CVProfCounter _counter(name);      \
    _counter.Increment(amount);               \
  } while (0)
#define VPROF_INCREMENT_GROUP_COUNTER(name, group, amount) \
  do {                                                     \
    static CVProfCounter _counter(name, group);            \
    _counter.Increment(amount);                            \
  } while (0)

#else

#define VPROF(name) ((void)0)
#define VPROF_ASSERT_ACCOUNTED(name) ((void)0)
#define VPROF_(name, detail, group, bAssertAccounted) ((void)0)
#define VPROF_BUDGET(name, group) ((void)0)
#define VPROF_BUDGET_FLAGS(name, group, flags) ((void)0)

#define VPROF_SCOPE_BEGIN(tag) do {
#define VPROF_SCOPE_END() \
  }                       \
  while (0)

#define VPROF_ONLY(expression) ((void)0)

#define VPROF_ENTER_SCOPE(name)
#define VPROF_EXIT_SCOPE()

#define VPROF_INCREMENT_COUNTER(name, amount) ((void)0)
#define VPROF_INCREMENT_GROUP_COUNTER(name, group, amount) ((void)0)

#endif

#ifdef VPROF_ENABLED

// A node in the call graph hierarchy
class SOURCE_TIER0_API_CLASS CVProfNode {
  friend class CVProfRecorder;
  friend class CVProfile;

 public:
  CVProfNode(const ch *pszName, i32 detailLevel, CVProfNode *pParent,
             const ch *pBudgetGroupName, i32 budgetFlags);
  ~CVProfNode() {
#ifndef OS_WIN
    delete m_pChild;
    delete m_pSibling;
#endif
  }

  CVProfNode *GetSubNode(const ch *pszName, i32 detailLevel,
                         const ch *pBudgetGroupName, i32 budgetFlags);
  CVProfNode *GetSubNode(const ch *pszName, i32 detailLevel,
                         const ch *pBudgetGroupName);
  CVProfNode *GetParent() const {
    Assert(m_pParent);
    return m_pParent;
  }
  CVProfNode *GetSibling() const { return m_pSibling; }
  CVProfNode *GetPrevSibling() const {
    CVProfNode *p = GetParent();

    if (!p) return nullptr;

    CVProfNode *s;
    for (s = p->GetChild(); s && (s->GetSibling() != this); s = s->GetSibling())
      ;

    return s;
  }
  CVProfNode *GetChild() const { return m_pChild; }

  void MarkFrame();
  void ResetPeak();

  void Pause();
  void Resume();
  void Reset();

  void EnterScope();
  bool ExitScope();

  const ch *GetName() const {
    Assert(m_pszName);
    return m_pszName;
  }

  i32 GetBudgetGroupID() const { return m_BudgetGroupID; }

  // Only used by the record/playback stuff.
  void SetBudgetGroupID(i32 id) { m_BudgetGroupID = id; }

  i32 GetCurCalls() const { return m_nCurFrameCalls; }
  f64 GetCurTime() const { return m_CurFrameTime.GetMillisecondsF(); }
  i32 GetPrevCalls() const { return m_nPrevFrameCalls; }
  f64 GetPrevTime() const { return m_PrevFrameTime.GetMillisecondsF(); }
  u32 GetTotalCalls() const { return m_nTotalCalls; }
  f64 GetTotalTime() const { return m_TotalTime.GetMillisecondsF(); }
  f64 GetPeakTime() const { return m_PeakTime.GetMillisecondsF(); }

  f64 GetCurTimeLessChildren() const {
    f64 result = GetCurTime();
    CVProfNode *pChild = GetChild();
    while (pChild) {
      result -= pChild->GetCurTime();
      pChild = pChild->GetSibling();
    }
    return result;
  }
  f64 GetPrevTimeLessChildren() const {
    f64 result = GetPrevTime();
    CVProfNode *pChild = GetChild();
    while (pChild) {
      result -= pChild->GetPrevTime();
      pChild = pChild->GetSibling();
    }
    return result;
  }
  f64 GetTotalTimeLessChildren() const {
    f64 result = GetTotalTime();
    CVProfNode *pChild = GetChild();
    while (pChild) {
      result -= pChild->GetTotalTime();
      pChild = pChild->GetSibling();
    }
    return result;
  }

  i32 GetPrevL2CacheMissLessChildren() const { return 0; }
  i32 GetPrevLoadHitStoreLessChildren() const { return 0; }

  void ClearPrevTime() { m_PrevFrameTime.Init(); }

  i32 GetL2CacheMisses() const { return 0; }

  // Not used in the common case...
  void SetCurFrameTime(unsigned long milliseconds);

  void SetClientData(i32 iClientData) { m_iClientData = iClientData; }
  i32 GetClientData() const { return m_iClientData; }

#ifdef DBGFLAG_VALIDATE
  void Validate(CValidator &validator,
                ch *pchName);  // Validate our internal structures
#endif                         // DBGFLAG_VALIDATE

  // Used by vprof record/playback.
 private:
  void SetUniqueNodeID(i32 id) { m_iUniqueNodeID = id; }

  i32 GetUniqueNodeID() const { return m_iUniqueNodeID; }

  static i32 s_iCurrentUniqueNodeID;

 private:
  const ch *m_pszName;
  CFastTimer m_Timer;

  i32 m_nRecursions;

  u32 m_nCurFrameCalls;
  CCycleCount m_CurFrameTime;

  u32 m_nPrevFrameCalls;
  CCycleCount m_PrevFrameTime;

  u32 m_nTotalCalls;
  CCycleCount m_TotalTime;

  CCycleCount m_PeakTime;

  CVProfNode *m_pParent;
  CVProfNode *m_pChild;
  CVProfNode *m_pSibling;

  i32 m_BudgetGroupID;

  i32 m_iClientData;
  i32 m_iUniqueNodeID;
};

//
// Coordinator and root node of the profile hierarchy tree
//

enum VProfReportType_t {
  VPRT_SUMMARY = (1 << 0),
  VPRT_HIERARCHY = (1 << 1),
  VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY = (1 << 2),
  VPRT_LIST_BY_TIME = (1 << 3),
  VPRT_LIST_BY_TIME_LESS_CHILDREN = (1 << 4),
  VPRT_LIST_BY_AVG_TIME = (1 << 5),
  VPRT_LIST_BY_AVG_TIME_LESS_CHILDREN = (1 << 6),
  VPRT_LIST_BY_PEAK_TIME = (1 << 7),
  VPRT_LIST_BY_PEAK_OVER_AVERAGE = (1 << 8),
  VPRT_LIST_TOP_ITEMS_ONLY = (1 << 9),

  VPRT_FULL = (0xffffffff & ~(VPRT_HIERARCHY_TIME_PER_FRAME_AND_COUNT_ONLY |
                              VPRT_LIST_TOP_ITEMS_ONLY)),
};

enum CounterGroup_t {
  COUNTER_GROUP_DEFAULT = 0,
  // The engine doesn't reset these counters. Usually,
  // they are used
  // like global variables that can be accessed across
  // modules.
  COUNTER_GROUP_NO_RESET,
  // Global texture usage counters (totals for
  // what is currently in memory).
  COUNTER_GROUP_TEXTURE_GLOBAL,
  // Per-frame texture usage counters.
  COUNTER_GROUP_TEXTURE_PER_FRAME
};

class SOURCE_TIER0_API_CLASS CVProfile {
 public:
  CVProfile();
  ~CVProfile();

  void Term();

  // Runtime operations

  void Start() {
    if (++m_enabled == 1) {
      m_Root.EnterScope();
    }
  }
  void Stop() {
    if (--m_enabled == 0) m_Root.ExitScope();
  }

  void EnterScope(const ch *pszName, i32 detailLevel,
                  const ch *pBudgetGroupName, bool bAssertAccounted) {
    EnterScope(pszName, detailLevel, pBudgetGroupName, bAssertAccounted,
               BUDGETFLAG_OTHER);
  }
  void EnterScope(const ch *pszName, i32 detailLevel,
                  const ch *pBudgetGroupName, bool bAssertAccounted,
                  i32 budgetFlags) {
    if ((m_enabled != 0 || !m_fAtRoot) &&
        ThreadInMainThread())  // if became disabled, need to unwind back to
                               // root before stopping
    {
      // Only account for vprof stuff on the primary thread.
      // if( !Plat_IsPrimaryThread() )
      //	return;

      if (pszName != m_pCurNode->GetName()) {
        m_pCurNode = m_pCurNode->GetSubNode(pszName, detailLevel,
                                            pBudgetGroupName, budgetFlags);
      }
      m_pBudgetGroups[m_pCurNode->GetBudgetGroupID()].m_BudgetFlags |=
          budgetFlags;

#if !defined(NDEBUG)
      // 360 doesn't want this to allow tier0 debug/release .def files to match
      if (bAssertAccounted) {
        // FIXME
        AssertOnce(m_pCurNode->GetBudgetGroupID() != 0);
      }
#endif
      m_pCurNode->EnterScope();
      m_fAtRoot = false;
    }
  }
  void ExitScope() {
    if ((!m_fAtRoot || m_enabled != 0) && ThreadInMainThread()) {
      // Only account for vprof stuff on the primary thread.
      // if( !Plat_IsPrimaryThread() )
      //	return;

      // ExitScope will indicate whether we should back up to our parent (we may
      // be profiling a recursive function)
      if (m_pCurNode->ExitScope()) {
        m_pCurNode = m_pCurNode->GetParent();
      }
      m_fAtRoot = (m_pCurNode == &m_Root);
    }
  }

  void MarkFrame() {
    if (m_enabled) {
      ++m_nFrames;
      m_Root.ExitScope();
      m_Root.MarkFrame();
      m_Root.EnterScope();
    }
  }
  void ResetPeaks() { m_Root.ResetPeak(); }

  void Pause() {
    m_pausedEnabledDepth = m_enabled;
    m_enabled = 0;
    if (!AtRoot()) m_Root.Pause();
  }
  void Resume() {
    m_enabled = m_pausedEnabledDepth;
    if (!AtRoot()) m_Root.Resume();
  }
  void Reset() {
    m_Root.Reset();
    m_nFrames = 0;
  }

  bool IsEnabled() const { return (m_enabled != 0); }
  i32 GetDetailLevel() const { return m_ProfileDetailLevel; }

  bool AtRoot() const { return m_fAtRoot; }

  // Queries

#ifdef VPROF_VTUNE_GROUP
#define MAX_GROUP_STACK_DEPTH 1024

  void EnableVTuneGroup(const ch *pGroupName) {
    m_nVTuneGroupID = BudgetGroupNameToBudgetGroupID(pGroupName);
    m_bVTuneGroupEnabled = true;
  }
  void DisableVTuneGroup() { m_bVTuneGroupEnabled = false; }

  inline void PushGroup(i32 nGroupID) {
    // There is always at least one item on the stack since we force
    // the first element to be VPROF_BUDGETGROUP_OTHER_UNACCOUNTED.
    Assert(m_GroupIDStackDepth > 0);
    Assert(m_GroupIDStackDepth < MAX_GROUP_STACK_DEPTH);
    m_GroupIDStack[m_GroupIDStackDepth] = nGroupID;
    m_GroupIDStackDepth++;
    if (m_GroupIDStack[m_GroupIDStackDepth - 2] != nGroupID &&
        VTuneGroupEnabled() && nGroupID == VTuneGroupID()) {
      vtune(true);
    }
  }
  inline void PopGroup() {
    m_GroupIDStackDepth--;
    // There is always at least one item on the stack since we force
    // the first element to be VPROF_BUDGETGROUP_OTHER_UNACCOUNTED.
    Assert(m_GroupIDStackDepth > 0);
    if (m_GroupIDStack[m_GroupIDStackDepth] !=
            m_GroupIDStack[m_GroupIDStackDepth + 1] &&
        VTuneGroupEnabled() &&
        m_GroupIDStack[m_GroupIDStackDepth + 1] == VTuneGroupID()) {
      vtune(false);
    }
  }
#endif

  i32 NumFramesSampled() const { return m_nFrames; }
  f64 GetPeakFrameTime() const { return m_Root.GetPeakTime(); }
  f64 GetTotalTimeSampled() const { return m_Root.GetTotalTime(); }
  f64 GetTimeLastFrame() const { return m_Root.GetCurTime(); }

  CVProfNode *GetRoot() { return &m_Root; }
  CVProfNode *FindNode(CVProfNode *pStartNode, const ch *pszNode);

  void OutputReport(i32 type = VPRT_FULL, const ch *pszStartNode = nullptr,
                    i32 budgetGroupID = -1);

  const ch *GetBudgetGroupName(i32 budgetGroupID) const {
    Assert(budgetGroupID >= 0 && budgetGroupID < m_nBudgetGroupNames);
    return m_pBudgetGroups[budgetGroupID].m_pName;
  }
  i32 GetBudgetGroupFlags(i32 budgetGroupID) const {
    Assert(budgetGroupID >= 0 && budgetGroupID < m_nBudgetGroupNames);
    return m_pBudgetGroups[budgetGroupID].m_BudgetFlags;
  }  // Returns a combination of BUDGETFLAG_ defines.
  i32 GetNumBudgetGroups();
  void GetBudgetGroupColor(i32 budgetGroupID, i32 &r, i32 &g, i32 &b, i32 &a);
  i32 BudgetGroupNameToBudgetGroupID(const ch *pBudgetGroupName);
  i32 BudgetGroupNameToBudgetGroupID(const ch *pBudgetGroupName,
                                     i32 budgetFlagsToORIn);
  void RegisterNumBudgetGroupsChangedCallBack(void (*pCallBack)(void));

  i32 BudgetGroupNameToBudgetGroupIDNoCreate(const ch *pBudgetGroupName) {
    return FindBudgetGroupName(pBudgetGroupName);
  }

  void HideBudgetGroup(i32 budgetGroupID, bool bHide = true);
  void HideBudgetGroup(const ch *pszName, bool bHide = true) {
    HideBudgetGroup(BudgetGroupNameToBudgetGroupID(pszName), bHide);
  }

  i32 *FindOrCreateCounter(
      const ch *pName, CounterGroup_t eCounterGroup = COUNTER_GROUP_DEFAULT);
  void ResetCounters(CounterGroup_t eCounterGroup);

  i32 GetNumCounters(void) const;

  const ch *GetCounterName(i32 index) const;
  i32 GetCounterValue(i32 index) const;
  const ch *GetCounterNameAndValue(i32 index, i32 &val) const;
  CounterGroup_t GetCounterGroup(i32 index) const;

  // Performance monitoring events.
  void PMEInitialized(bool bInit) { m_bPMEInit = bInit; }
  void PMEEnable(bool bEnable) { m_bPMEEnabled = bEnable; }

  bool UsePME() const { return (m_bPMEInit && m_bPMEEnabled); }

#ifdef DBGFLAG_VALIDATE
  void Validate(CValidator &validator,
                ch *pchName);  // Validate our internal structures
#endif                         // DBGFLAG_VALIDATE

 protected:
  void FreeNodes_R(CVProfNode *pNode);

#ifdef VPROF_VTUNE_GROUP
  bool VTuneGroupEnabled() const { return m_bVTuneGroupEnabled; }
  i32 VTuneGroupID() const { return m_nVTuneGroupID; }
#endif

  void SumTimes(const ch *pszStartNode, i32 budgetGroupID);
  void SumTimes(CVProfNode *pNode, i32 budgetGroupID);
  void DumpNodes(CVProfNode *pNode, i32 indent, bool bAverageAndCountOnly);
  i32 FindBudgetGroupName(const ch *pBudgetGroupName);
  i32 AddBudgetGroupName(const ch *pBudgetGroupName, i32 budgetFlags);

#ifdef VPROF_VTUNE_GROUP
  bool m_bVTuneGroupEnabled;
  i32 m_nVTuneGroupID;
  i32 m_GroupIDStack[MAX_GROUP_STACK_DEPTH];
  i32 m_GroupIDStackDepth;
#endif
  i32 m_enabled;
  bool m_fAtRoot;  // tracked for efficiency of the "not profiling" case
  CVProfNode *m_pCurNode;
  CVProfNode m_Root;
  i32 m_nFrames;
  i32 m_ProfileDetailLevel;
  i32 m_pausedEnabledDepth;

  class CBudgetGroup {
   public:
    ch *m_pName;
    i32 m_BudgetFlags;
  };

  CBudgetGroup *m_pBudgetGroups;
  i32 m_nBudgetGroupNamesAllocated;
  i32 m_nBudgetGroupNames;
  void (*m_pNumBudgetGroupsChangedCallBack)();

  // Performance monitoring events.
  bool m_bPMEInit;
  bool m_bPMEEnabled;

  i32 m_Counters[MAXCOUNTERS];
  ch m_CounterGroups[MAXCOUNTERS];  // (These are CounterGroup_t's).
  ch *m_CounterNames[MAXCOUNTERS];
  i32 m_NumCounters;
};

SOURCE_TIER0_API CVProfile g_VProfCurrentProfile;

inline CVProfNode::CVProfNode(const ch *pszName, i32 detailLevel,
                              CVProfNode *pParent, const ch *pBudgetGroupName,
                              i32 budgetFlags)
    : m_pszName{pszName},
      m_nCurFrameCalls{0},
      m_nPrevFrameCalls{0},
      m_nRecursions{0},
      m_pParent{pParent},
      m_pChild{nullptr},
      m_pSibling{nullptr},
      m_iClientData{-1} {
  m_iUniqueNodeID = s_iCurrentUniqueNodeID++;

  if (m_iUniqueNodeID > 0) {
    m_BudgetGroupID = g_VProfCurrentProfile.BudgetGroupNameToBudgetGroupID(
        pBudgetGroupName, budgetFlags);
  } else {
    // "m_Root" can't call BudgetGroupNameToBudgetGroupID
    // because g_VProfCurrentProfile not yet initialized
    m_BudgetGroupID = 0;
  }

  Reset();

  if (m_pParent && (m_BudgetGroupID == VPROF_BUDGET_GROUP_ID_UNACCOUNTED)) {
    m_BudgetGroupID = m_pParent->GetBudgetGroupID();
  }
}

class CVProfScope {
 public:
  CVProfScope(const ch *pszName, i32 detailLevel, const ch *pBudgetGroupName,
              bool bAssertAccounted, i32 budgetFlags) {
    g_VProfCurrentProfile.EnterScope(pszName, detailLevel, pBudgetGroupName,
                                     bAssertAccounted, budgetFlags);
  }
  ~CVProfScope() { g_VProfCurrentProfile.ExitScope(); }
};

class CVProfCounter {
 public:
  CVProfCounter(const ch *pName, CounterGroup_t group = COUNTER_GROUP_DEFAULT) {
    m_pCounter = g_VProfCurrentProfile.FindOrCreateCounter(pName, group);
    Assert(m_pCounter);
  }
  ~CVProfCounter() {}

  void Increment(i32 val) {
    Assert(m_pCounter);
    *m_pCounter += val;
  }

 private:
  i32 *m_pCounter;
};

#endif

#endif  // SOURCE_TIER0_INCLUDE_VPROF_H_
