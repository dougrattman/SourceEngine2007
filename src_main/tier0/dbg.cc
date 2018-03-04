// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/dbg.h"

#include <cassert>
#include "public/Color.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/minidump.h"
#include "tier0/include/threadtools.h"
#include "tier0/include/wchartypes.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

enum { MAX_GROUP_NAME_LENGTH = 48 };

struct SpewGroup_t {
  ch m_GroupName[MAX_GROUP_NAME_LENGTH];
  i32 m_Level;
};

SpewRetval_t DefaultSpewFunc(SpewType_t type, const ch* pMsg) {
  printf("%s", pMsg);
#ifdef OS_WIN
  Plat_DebugString(pMsg);
#endif

  if (type == SPEW_ASSERT) return SPEW_DEBUGGER;
  if (type == SPEW_ERROR) return SPEW_ABORT;

  return SPEW_CONTINUE;
}

static SpewOutputFunc_t s_SpewOutputFunc = DefaultSpewFunc;

static const ch* s_pFileName;
static i32 s_Line;
static SpewType_t s_SpewType;

static SpewGroup_t* s_pSpewGroups = 0;
static i32 s_GroupCount = 0;
static i32 s_DefaultLevel = 0;
static Color s_DefaultOutputColor(255, 255, 255, 255);

// Only useable from within a spew function
struct SpewInfo_t {
  const Color* m_pSpewOutputColor;
  const ch* m_pSpewOutputGroup;
  i32 m_nSpewOutputLevel;
};

CThreadLocalPtr<SpewInfo_t> g_pSpewInfo;

// Standard groups
static const ch* s_pDeveloper = "developer";
static const ch* s_pConsole = "console";
static const ch* s_pNetwork = "network";

enum StandardSpewGroup_t {
  GROUP_DEVELOPER = 0,
  GROUP_CONSOLE,
  GROUP_NETWORK,

  GROUP_COUNT,
};

static i32 s_pGroupIndices[GROUP_COUNT] = {-1, -1, -1};
static const ch* s_pGroupNames[GROUP_COUNT] = {s_pDeveloper, s_pConsole,
                                               s_pNetwork};

// Spew output management.
void SpewOutputFunc(SpewOutputFunc_t func) {
  s_SpewOutputFunc = func ? func : DefaultSpewFunc;
}

SpewOutputFunc_t GetSpewOutputFunc() {
  if (s_SpewOutputFunc) return s_SpewOutputFunc;
  return DefaultSpewFunc;
}

void _ExitOnFatalAssert(const ch* pFile, i32 line) {
  _SpewMessage("Fatal assert failed: %s, line %d.  Application exiting.\n",
               pFile, line);

  // only write out minidumps if we're not in the debugger
  if (!Plat_IsInDebugSession()) {
    WriteMiniDump();
  }

  DevMsg(1, "_ExitOnFatalAssert\n");
  exit(EXIT_FAILURE);
}

// Templates to assist in validating pointers:

SOURCE_TIER0_API void _AssertValidReadPtr(void* ptr, i32 count /* = 1*/) {}

SOURCE_TIER0_API void _AssertValidWritePtr(void* ptr, i32 count /* = 1*/) {}

SOURCE_TIER0_API void _AssertValidReadWritePtr(void* ptr, i32 count /* = 1*/) {}

SOURCE_TIER0_API void AssertValidStringPtr(const ch* ptr,
                                           i32 maxchar /* = 0xFFFFFF */) {
  Assert(ptr);
}

// Should be called only inside a SpewOutputFunc_t, returns groupname, level,
// color

const ch* GetSpewOutputGroup() {
  SpewInfo_t* pSpewInfo = g_pSpewInfo;
  assert(pSpewInfo);
  if (pSpewInfo) return pSpewInfo->m_pSpewOutputGroup;
  return nullptr;
}

i32 GetSpewOutputLevel() {
  SpewInfo_t* pSpewInfo = g_pSpewInfo;
  assert(pSpewInfo);
  if (pSpewInfo) return pSpewInfo->m_nSpewOutputLevel;
  return -1;
}

const Color& GetSpewOutputColor() {
  SpewInfo_t* pSpewInfo = g_pSpewInfo;
  assert(pSpewInfo);
  if (pSpewInfo) return *pSpewInfo->m_pSpewOutputColor;
  return s_DefaultOutputColor;
}

// Spew functions

void _SpewInfo(SpewType_t type, const ch* pFile, i32 line) {
  // Only grab the file name. Ignore the path.
  const ch* pSlash = strrchr(pFile, '\\');
  const ch* pSlash2 = strrchr(pFile, '/');
  if (pSlash < pSlash2) pSlash = pSlash2;

  s_pFileName = pSlash ? pSlash + 1 : pFile;
  s_Line = line;
  s_SpewType = type;
}

static SpewRetval_t _SpewMessage(SpewType_t spewType, const ch* pGroupName,
                                 i32 nLevel, const Color* pColor,
                                 const ch* pMsgFormat, va_list args) {
  ch message[5020];
  // check that we won't artifically truncate the string
  assert(strlen(pMsgFormat) < ARRAYSIZE(message));

  /* Printf the file and line for warning + assert only... */
  i32 len = 0;
  if ((spewType == SPEW_ASSERT)) {
    len = _snprintf_s(message, ARRAYSIZE(message) - 1,
                      "%s (%d) : ", s_pFileName, s_Line);
  }

  if (len == -1) return SPEW_ABORT;

  /* Create the message.... */
  i32 val =
      _vsnprintf(&message[len], ARRAYSIZE(message) - len - 1, pMsgFormat, args);
  if (val == -1) return SPEW_ABORT;

  len += val;
  /* use normal assert here; to avoid recursion. */
  assert(len * sizeof(*pMsgFormat) < ARRAYSIZE(message));

  // Add \n for warning and assert
  if ((spewType == SPEW_ASSERT)) {
    len += sprintf(&message[len], "\n");
  }

  /* use normal assert here; to avoid recursion. */
  assert(len < ARRAYSIZE(message) - 1);
  assert(s_SpewOutputFunc);

  /* direct it to the appropriate target(s) */
  assert(static_cast<const SpewInfo_t*>(g_pSpewInfo) == nullptr);
  SpewInfo_t spewInfo = {pColor, pGroupName, nLevel};

  g_pSpewInfo = &spewInfo;
  SpewRetval_t ret = s_SpewOutputFunc(spewType, message);
  g_pSpewInfo = nullptr;

  switch (ret) {
    // Asserts put the break into the macro so it occurs in the right place
    case SPEW_DEBUGGER:
      if (spewType != SPEW_ASSERT) {
        DebuggerBreak();
      }
      break;

    case SPEW_ABORT:
      ConMsg("Exiting on SPEW_ABORT\n");
      exit(0);

    default:
      break;
  }

  return ret;
}

#include "tier0/include/valve_off.h"

FORCEINLINE SpewRetval_t _SpewMessage(SpewType_t spewType, const ch* pMsgFormat,
                                      va_list args) {
  return _SpewMessage(spewType, "", 0, &s_DefaultOutputColor, pMsgFormat, args);
}

// Find a group, return true if found, false if not. Return in ind the
// index of the found group, or the index of the group right before where the
// group should be inserted into the list to maintain sorted order.

bool FindSpewGroup(const ch* pGroupName, i32* pInd) {
  i32 s = 0;
  if (s_GroupCount) {
    i32 e = (i32)(s_GroupCount - 1);
    while (s <= e) {
      i32 m = (s + e) >> 1;
      i32 cmp = stricmp(pGroupName, s_pSpewGroups[m].m_GroupName);
      if (!cmp) {
        *pInd = m;
        return true;
      }
      if (cmp < 0)
        e = m - 1;
      else
        s = m + 1;
    }
  }
  *pInd = s;
  return false;
}

// Tests to see if a particular spew is active

bool IsSpewActive(const ch* pGroupName, i32 level) {
  // If we don't find the spew group, use the default level.
  i32 ind;
  if (FindSpewGroup(pGroupName, &ind))
    return s_pSpewGroups[ind].m_Level >= level;
  else
    return s_DefaultLevel >= level;
}

inline bool IsSpewActive(StandardSpewGroup_t group, i32 level) {
  // If we don't find the spew group, use the default level.
  if (s_pGroupIndices[group] >= 0)
    return s_pSpewGroups[s_pGroupIndices[group]].m_Level >= level;
  return s_DefaultLevel >= level;
}

SpewRetval_t _SpewMessage(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewRetval_t ret = _SpewMessage(s_SpewType, pMsgFormat, args);
  va_end(args);
  return ret;
}

SpewRetval_t _DSpewMessage(const ch* pGroupName, i32 level,
                           const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return SPEW_CONTINUE;

  va_list args;
  va_start(args, pMsgFormat);
  SpewRetval_t ret = _SpewMessage(s_SpewType, pGroupName, level,
                                  &s_DefaultOutputColor, pMsgFormat, args);
  va_end(args);
  return ret;
}

SOURCE_TIER0_API SpewRetval_t ColorSpewMessage(SpewType_t type,
                                               const Color* pColor,
                                               const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewRetval_t ret = _SpewMessage(type, "", 0, pColor, pMsgFormat, args);
  va_end(args);
  return ret;
}

void Msg(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, pMsgFormat, args);
  va_end(args);
}

void DMsg(const ch* pGroupName, i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, pGroupName, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void Warning(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, pMsgFormat, args);
  va_end(args);
}

void DWarning(const ch* pGroupName, i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, pGroupName, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void Log(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, pMsgFormat, args);
  va_end(args);
}

void DLog(const ch* pGroupName, i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, pGroupName, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void Error(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_ERROR, pMsgFormat, args);
  va_end(args);
}

// A couple of super-common dynamic spew messages, here for convenience
// These looked at the "developer" group, print if it's level 1 or higher

void DevMsg(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pDeveloper, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void DevWarning(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, s_pDeveloper, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void DevLog(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, s_pDeveloper, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void DevMsg(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void DevWarning(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void DevLog(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

// A couple of super-common dynamic spew messages, here for convenience
// These looked at the "console" group, print if it's level 1 or higher

void ConColorMsg(i32 level, const Color& clr, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pConsole, level, &clr, pMsgFormat, args);
  va_end(args);
}

void ConMsg(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pConsole, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void ConWarning(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, s_pConsole, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void ConLog(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, s_pConsole, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConColorMsg(const Color& clr, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pConsole, 1, &clr, pMsgFormat, args);
  va_end(args);
}

void ConMsg(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConWarning(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConLog(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConDColorMsg(const Color& clr, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pConsole, 2, &clr, pMsgFormat, args);
  va_end(args);
}

void ConDMsg(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConDWarning(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConDLog(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

// A couple of super-common dynamic spew messages, here for convenience
// These looked at the "network" group, print if it's level 1 or higher

void NetMsg(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_NETWORK, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_MESSAGE, s_pNetwork, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void NetWarning(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_NETWORK, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_WARNING, s_pNetwork, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void NetLog(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_NETWORK, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  _SpewMessage(SPEW_LOG, s_pNetwork, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

#include "tier0/include/valve_on.h"

// Sets the priority level for a spew group

void SpewActivate(const ch* pGroupName, i32 level) {
  Assert(pGroupName);

  // check for the default group first...
  if ((pGroupName[0] == '*') && (pGroupName[1] == '\0')) {
    s_DefaultLevel = level;
    return;
  }

  // Normal case, search in group list using binary search.
  // If not found, grow the list of groups and insert it into the
  // right place to maintain sorted order. Then set the level.
  i32 ind;
  if (!FindSpewGroup(pGroupName, &ind)) {
    // not defined yet, insert an entry.
    ++s_GroupCount;
    if (s_pSpewGroups) {
      auto new_spew_groups = (SpewGroup_t*)PvRealloc(
          s_pSpewGroups, s_GroupCount * sizeof(SpewGroup_t));
      if (!new_spew_groups) {
        Assert(0);
        return;
      }

      s_pSpewGroups = new_spew_groups;

      // shift elements down to preserve order
      i32 numToMove = s_GroupCount - ind - 1;
      memmove(&s_pSpewGroups[ind + 1], &s_pSpewGroups[ind],
              numToMove * sizeof(SpewGroup_t));

      // Update standard groups
      for (i32 i = 0; i < GROUP_COUNT; ++i) {
        if ((ind <= s_pGroupIndices[i]) && (s_pGroupIndices[i] >= 0)) {
          ++s_pGroupIndices[i];
        }
      }
    } else {
      s_pSpewGroups = (SpewGroup_t*)PvAlloc(s_GroupCount * sizeof(SpewGroup_t));
      if (!s_pSpewGroups) {
        Assert(0);
        return;
      }
    }

    Assert(strlen(pGroupName) < MAX_GROUP_NAME_LENGTH);
    strcpy(s_pSpewGroups[ind].m_GroupName, pGroupName);

    // Update standard groups
    for (i32 i = 0; i < GROUP_COUNT; ++i) {
      if ((s_pGroupIndices[i] < 0) && !stricmp(s_pGroupNames[i], pGroupName)) {
        s_pGroupIndices[i] = ind;
        break;
      }
    }
  }
  s_pSpewGroups[ind].m_Level = level;
}

bool Plat_SimpleLog(const ch* file, i32 line) {
  FILE* f = fopen("simple.log", "at+");
  bool ok = !!f;
  if (ok) {
    ok = fprintf(f, "%s:%i\n", file, line) != 0;
    fclose(f);
  }
  return ok;
}

#ifdef DBGFLAG_VALIDATE
void ValidateSpew(CValidator& validator) {
  validator.Push(_T("Spew globals"), nullptr, _T("Global"));

  validator.ClaimMemory(s_pSpewGroups);

  validator.Pop();
}
#endif  // DBGFLAG_VALIDATE

// Purpose: For debugging startup times, etc.-
void COM_TimestampedLog(ch const* fmt, ...) {
  static f64 last_stamp = 0.0;
  static bool should_log = false;
  static bool is_checked = false;
  static bool is_first_time_write = false;

  if (!is_checked) {
    should_log = CommandLine()->CheckParm("-profile") ? true : false;
    is_checked = true;
  }
  if (!should_log) {
    return;
  }

  ch log_buffer[1024];
  va_list arg_list;
  va_start(arg_list, fmt);
  _vsnprintf_s(log_buffer, ARRAYSIZE(log_buffer), fmt, arg_list);
  va_end(arg_list);

  const f64 curent_stamp = Plat_FloatTime();

  if (!is_first_time_write) {
    _unlink("timestamped.log");
    is_first_time_write = true;
  }

  FILE* f = fopen("timestamped.log", "at+");
  if (f) {
    fprintf(f, "%8.4f / %8.4f:  %s\n", curent_stamp, curent_stamp - last_stamp,
            log_buffer);
    fclose(f);
  }

  last_stamp = curent_stamp;
}
