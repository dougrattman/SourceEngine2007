// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/dbg.h"

#include <fcntl.h>
#include <io.h>
#include <cassert>
#include <cstdio>

#include "public/Color.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/minidump.h"
#include "tier0/include/platform.h"
#include "tier0/include/threadtools.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#endif

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
static usize s_GroupCount = 0;
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

void ExitOnFatalAssert_(const ch* pFile, i32 line) {  //-V524
  SpewMessage_("Fatal assert failed: %s, line %d.  Application exiting.\n",
               pFile, line);

  // only write out minidumps if we're not in the debugger
  if (!Plat_IsInDebugSession()) {
    WriteMiniDump();
  }

  DevMsg(1, "ExitOnFatalAssert_\n");
  exit(EXIT_FAILURE);
}

// Templates to assist in validating pointers:

SOURCE_TIER0_API void _AssertValidWritePtr(void* ptr, i32 count /* = 1*/) {}

SOURCE_TIER0_API void AssertValidStringPtr(const ch* ptr,
                                           i32 maxchar /* = 0xFFFFFF */) {}

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

void SpewInfo_(SpewType_t type, const ch* pFile, i32 line) {  //-V524
  // Only grab the file name. Ignore the path.
  const ch* pSlash = strrchr(pFile, '\\');
  const ch* pSlash2 = strrchr(pFile, '/');
  if (pSlash < pSlash2) pSlash = pSlash2;

  s_pFileName = pSlash ? pSlash + 1 : pFile;
  s_Line = line;
  s_SpewType = type;
}

static SpewRetval_t SpewMessage_(SpewType_t spewType, const ch* pGroupName,
                                 i32 nLevel, const Color* pColor,
                                 const ch* pMsgFormat, va_list args) {
  ch message[5020];
  // check that we won't artifically truncate the string
  assert(strlen(pMsgFormat) < SOURCE_ARRAYSIZE(message));

  /* Printf the file and line for warning + assert only... */
  i32 len = 0;
  if ((spewType == SPEW_ASSERT)) {
    len = _snprintf_s(message, SOURCE_ARRAYSIZE(message) - 1,
                      "%s (%d) : ", s_pFileName, s_Line);
  }

  if (len == -1) return SPEW_ABORT;

  /* Create the message.... */
  i32 val = _vsnprintf_s(&message[len], SOURCE_ARRAYSIZE(message) - len,
                         SOURCE_ARRAYSIZE(message) - len - 1, pMsgFormat, args);
  if (val == -1) return SPEW_ABORT;

  len += val;
  /* use normal assert here; to avoid recursion. */
  assert(len * sizeof(*pMsgFormat) < SOURCE_ARRAYSIZE(message));

  // Add \n for warning and assert
  if ((spewType == SPEW_ASSERT)) {
    len += sprintf_s(&message[len], SOURCE_ARRAYSIZE(message) - len, "\n");
  }

  /* use normal assert here; to avoid recursion. */
  assert(len < SOURCE_ARRAYSIZE(message) - 1);
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

SOURCE_FORCEINLINE SpewRetval_t SpewMessage_(SpewType_t spewType,
                                             const ch* pMsgFormat,
                                             va_list args) {
  return SpewMessage_(spewType, "", 0, &s_DefaultOutputColor, pMsgFormat, args);
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
      i32 cmp = _stricmp(pGroupName, s_pSpewGroups[m].m_GroupName);
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

SpewRetval_t SpewMessage_(const ch* pMsgFormat, ...) {  //-V524
  va_list args;
  va_start(args, pMsgFormat);
  SpewRetval_t ret = SpewMessage_(s_SpewType, pMsgFormat, args);
  va_end(args);
  return ret;
}

SpewRetval_t DSpewMessage_(const ch* pGroupName, i32 level,  //-V524
                           const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return SPEW_CONTINUE;

  va_list args;
  va_start(args, pMsgFormat);
  SpewRetval_t ret = SpewMessage_(s_SpewType, pGroupName, level,
                                  &s_DefaultOutputColor, pMsgFormat, args);
  va_end(args);
  return ret;
}

SOURCE_TIER0_API SpewRetval_t ColorSpewMessage(SpewType_t type,
                                               const Color* pColor,
                                               const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewRetval_t ret = SpewMessage_(type, "", 0, pColor, pMsgFormat, args);
  va_end(args);
  return ret;
}

void Msg(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, pMsgFormat, args);
  va_end(args);
}

void DMsg(const ch* pGroupName, i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, pGroupName, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void Warning(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, pMsgFormat, args);
  va_end(args);
}

void DWarning(const ch* pGroupName, i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, pGroupName, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void Log(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, pMsgFormat, args);
  va_end(args);
}

void DLog(const ch* pGroupName, i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(pGroupName, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, pGroupName, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void Error(const ch* pMsgFormat, ...) {
  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_ERROR, pMsgFormat, args);
  va_end(args);
}

// A couple of super-common dynamic spew messages, here for convenience
// These looked at the "developer" group, print if it's level 1 or higher

void DevMsg(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pDeveloper, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void DevWarning(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, s_pDeveloper, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void DevLog(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, s_pDeveloper, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void DevMsg(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void DevWarning(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void DevLog(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_DEVELOPER, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, s_pDeveloper, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

// A couple of super-common dynamic spew messages, here for convenience
// These looked at the "console" group, print if it's level 1 or higher

void ConColorMsg(i32 level, const Color& clr, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pConsole, level, &clr, pMsgFormat, args);
  va_end(args);
}

void ConMsg(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pConsole, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void ConWarning(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, s_pConsole, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void ConLog(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, s_pConsole, level, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConColorMsg(const Color& clr, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pConsole, 1, &clr, pMsgFormat, args);
  va_end(args);
}

void ConMsg(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConWarning(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConLog(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 1)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, s_pConsole, 1, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConDColorMsg(const Color& clr, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pConsole, 2, &clr, pMsgFormat, args);
  va_end(args);
}

void ConDMsg(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConDWarning(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

void ConDLog(const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_CONSOLE, 2)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, s_pConsole, 2, &s_DefaultOutputColor, pMsgFormat,
               args);
  va_end(args);
}

// A couple of super-common dynamic spew messages, here for convenience
// These looked at the "network" group, print if it's level 1 or higher

void NetMsg(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_NETWORK, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_MESSAGE, s_pNetwork, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void NetWarning(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_NETWORK, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_WARNING, s_pNetwork, level, &s_DefaultOutputColor,
               pMsgFormat, args);
  va_end(args);
}

void NetLog(i32 level, const ch* pMsgFormat, ...) {
  if (!IsSpewActive(GROUP_NETWORK, level)) return;

  va_list args;
  va_start(args, pMsgFormat);
  SpewMessage_(SPEW_LOG, s_pNetwork, level, &s_DefaultOutputColor, pMsgFormat,
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
      usize numToMove = s_GroupCount - ind - 1;
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
    strcpy_s(s_pSpewGroups[ind].m_GroupName, pGroupName);

    // Update standard groups
    for (i32 i = 0; i < GROUP_COUNT; ++i) {
      if ((s_pGroupIndices[i] < 0) && !_stricmp(s_pGroupNames[i], pGroupName)) {
        s_pGroupIndices[i] = ind;
        break;
      }
    }
  }
  s_pSpewGroups[ind].m_Level = level;
}

#ifdef DBGFLAG_VALIDATE
void ValidateSpew(CValidator& validator) {
  validator.Push(_T("Spew globals"), nullptr, _T("Global"));

  validator.ClaimMemory(s_pSpewGroups);

  validator.Pop();
}
#endif  // DBGFLAG_VALIDATE

#ifdef OS_WIN
bool SetupWin32ConsoleIO() {
  // Only useful on Windows platforms
  bool newConsole{false};

  if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
      newConsole = true;
      AllocConsole();
    }

    *stdout = *_fdopen(_open_osfhandle(reinterpret_cast<intptr_t>(
                                           GetStdHandle(STD_OUTPUT_HANDLE)),
                                       _O_TEXT),
                       "w");
    setvbuf(stdout, nullptr, _IONBF, 0);

    *stdin =
        *_fdopen(_open_osfhandle(
                     reinterpret_cast<intptr_t>(GetStdHandle(STD_INPUT_HANDLE)),
                     _O_TEXT),
                 "r");
    setvbuf(stdin, nullptr, _IONBF, 0);

    *stderr =
        *_fdopen(_open_osfhandle(
                     reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE)),
                     _O_TEXT),
                 "w");
    setvbuf(stdout, nullptr, _IONBF, 0);

    std::ios_base::sync_with_stdio();
  }

  return newConsole;
}
#endif
