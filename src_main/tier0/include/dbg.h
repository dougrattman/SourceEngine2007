// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_DBG_H_
#define SOURCE_TIER0_INCLUDE_DBG_H_

#include <cstdarg>
#include <cstdio>

#include "base/include/base_types.h"
#include "build/include/build_config.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbgflag.h"
#include "tier0/include/platform.h"
#include "tier0/include/tier0_api.h"

// Usage model for the Dbg library
//
// 1. Spew.
//
//   Spew can be used in a static and a dynamic mode. The static
//   mode allows us to display assertions and other messages either only
//   in debug builds, or in non-release builds. The dynamic mode allows us to
//   turn on and off certain spew messages while the application is running.
//
//   Static Spew messages:
//
//     Assertions are used to detect and warn about invalid states
//     Spews are used to display a particular status/warning message.
//
//     To use an assertion, use
//
//     Assert( (f == 5) );
//     AssertMsg( (f == 5), ("F needs to be %d here!\n", 5) );
//     AssertFunc( (f == 5), BadFunc() );
//     AssertEquals( f, 5 );
//     AssertFloatEquals( f, 5.0f, 1e-3 );
//
//     The first will simply report that an assertion failed on a particular
//     code file and line. The second version will display a print-f formatted
//     message
//     along with the file and line, the third will display a generic message
//     and will also cause the function BadFunc to be executed, and the last two
//     will report an error if f is not equal to 5 (the last one asserts within
//     a particular tolerance).
//
//     To use a warning, use
//
//     Warning("Oh I feel so %s all over\n", "yummy");
//
//     Warning will do its magic in only Debug builds. To perform spew in *all*
//     builds, use RelWarning.
//
//     Three other spew types, Msg, Log, and Error, are compiled into all
//     builds. These error types do *not* need two sets of parenthesis.
//
//     Msg( "Isn't this exciting %d?", 5 );
//     Error( "I'm just thrilled" );
//
//   Dynamic Spew messages
//
//     It is possible to dynamically turn spew on and off. Dynamic spew is
//     identified by a spew group and priority level. To turn spew on for a
//     particular spew group, use SpewActivate( "group", level ). This will
//     cause all spew in that particular group with priority levels <= the
//     level specified in the SpewActivate function to be printed. Use DSpew
//     to perform the spew:
//
//     DWarning( "group", level, "Oh I feel even yummier!\n" );
//
//     Priority level 0 means that the spew will *always* be printed, and group
//     '*' is the default spew group. If a DWarning is encountered using a group
//     whose priority has not been set, it will use the priority of the default
//     group. The priority of the default group is initially set to 0.
//
//   Spew output
//
//     The output of the spew system can be redirected to an externally-supplied
//     function which is responsible for outputting the spew. By default, the
//     spew is simply printed using printf.
//
//     To redirect spew output, call SpewOutput.
//
//     SpewOutputFunc( OutputFunc );
//
//     This will cause OutputFunc to be called every time a spew message is
//     generated. OutputFunc will be passed a spew type and a message to print.
//     It must return a value indicating whether the debugger should be invoked,
//     whether the program should continue running, or whether the program
//     should abort.
//
// 2. Code activation
//
//   To cause code to be run only in debug builds, use DBG_CODE:
//   An example is below.
//
//   DBG_CODE(
//   {
//      i32 x = 5;
//      ++x;
//   });
//
//   Code can be activated based on the dynamic spew groups also. Use
//
//   DBG_DCODE( "group", level,
//              { i32 x = 5; ++x; }
//            );
//
// 3. Breaking into the debugger.
//
//   To cause an unconditional break into the debugger in debug builds only, use
//   DBG_BREAK
//
//   DBG_BREAK();
//
//   You can force a break in any build (release or debug) using
//
//   DebuggerBreak();

class Color;

// Various types of spew messages.
// I'm sure you're asking yourself why SPEW_ instead of DBG_ ? It's because DBG_
// is used all over the place in windows.h For example, DBG_CONTINUE is defined.
// Feh.
enum SpewType_t {
  SPEW_MESSAGE = 0,
  SPEW_WARNING,
  SPEW_ASSERT,
  SPEW_ERROR,
  SPEW_LOG,

  SPEW_TYPE_COUNT
};

enum SpewRetval_t { SPEW_DEBUGGER = 0, SPEW_CONTINUE, SPEW_ABORT };

// Type of externally defined function used to display debug spew.
using SpewOutputFunc_t = SpewRetval_t (*)(SpewType_t spew_type, const ch *message);

// Used to redirect spew output.
SOURCE_TIER0_API void SpewOutputFunc(SpewOutputFunc_t func);

// Used to get the current spew output function.
SOURCE_TIER0_API SpewOutputFunc_t GetSpewOutputFunc();

// Should be called only inside a SpewOutputFunc_t, returns groupname, level,
// color.
SOURCE_TIER0_API const ch *GetSpewOutputGroup();
SOURCE_TIER0_API i32 GetSpewOutputLevel();
SOURCE_TIER0_API const Color &GetSpewOutputColor();

// Used to manage spew groups and subgroups.
SOURCE_TIER0_API void SpewActivate(const ch *group_name, i32 level);
SOURCE_TIER0_API bool IsSpewActive(const ch *group_name, i32 level);

// Used to display messages, should never be called directly.
SOURCE_TIER0_API void _SpewInfo(SpewType_t spew_type, const ch *file, i32 line);
SOURCE_TIER0_API SpewRetval_t _SpewMessage(const ch *format, ...);
SOURCE_TIER0_API SpewRetval_t _DSpewMessage(const ch *group_name, i32 level, const ch *format, ...);
SOURCE_TIER0_API SpewRetval_t ColorSpewMessage(SpewType_t spew_type, const Color *pColor,
                                               const ch *format, ...);
SOURCE_TIER0_API void _ExitOnFatalAssert(const ch *file, i32 line);
SOURCE_TIER0_API bool ShouldUseNewAssertDialog();

SOURCE_TIER0_API bool SetupWin32ConsoleIO();

// Returns true if they want to break in the debugger.
SOURCE_TIER0_API bool DoNewAssertDialog(const ch *file, i32 line, const ch *expression);

// Used to define macros, never use these directly.
#define _AssertMsg(_exp, _msg, _executeExp, _bFatal)                                    \
  do {                                                                                  \
    if (!(_exp)) {                                                                      \
      _SpewInfo(SPEW_ASSERT, __FILE__, __LINE__);                                       \
      SpewRetval_t spew = _SpewMessage("%s", _msg);                                     \
      _executeExp;                                                                      \
      if (spew == SPEW_DEBUGGER) {                                                      \
        if (!ShouldUseNewAssertDialog() || DoNewAssertDialog(__FILE__, __LINE__, _msg)) \
          DebuggerBreak();                                                              \
        if (_bFatal) _ExitOnFatalAssert(__FILE__, __LINE__);                            \
      }                                                                                 \
    }                                                                                   \
  } while (0)

#define _AssertMsgOnce(_exp, _msg, _bFatal)                \
  do {                                                     \
    static bool fAsserted;                                 \
    if (!fAsserted) {                                      \
      _AssertMsg(_exp, _msg, (fAsserted = true), _bFatal); \
    }                                                      \
  } while (0)

// Spew macros...

// AssertFatal macros
// AssertFatal is used to detect an unrecoverable error condition.
// If enabled, it may display an assert dialog (if DBGFLAG_ASSERTDLG is turned
// on or running under the debugger), and always terminates the application.

#ifdef DBGFLAG_ASSERTFATAL

#define AssertFatal(_exp) _AssertMsg(_exp, "Assertion Failed: " #_exp, ((void)0), true)
#define AssertFatalOnce(_exp) _AssertMsgOnce(_exp, "Assertion Failed: " #_exp, true)
#define AssertFatalMsg(_exp, _msg) _AssertMsg(_exp, _msg, ((void)0), true)
#define AssertFatalMsgOnce(_exp, _msg) _AssertMsgOnce(_exp, _msg, true)
#define AssertFatalFunc(_exp, _f) _AssertMsg(_exp, "Assertion Failed: " #_exp, _f, true)
#define AssertFatalEquals(_exp, _expectedValue) \
  AssertFatalMsg2((_exp) == (_expectedValue), "Expected %d but got %d!", (_expectedValue), (_exp))
#define AssertFatalFloatEquals(_exp, _expectedValue, _tol)                              \
  AssertFatalMsg2(fabs((_exp) - (_expectedValue)) <= (_tol), "Expected %f but got %f!", \
                  (_expectedValue), (_exp))
#define VerifyFatal(_exp) AssertFatal(_exp)
#define VerifyEqualsFatal(_exp, _expectedValue) AssertFatalEquals(_exp, _expectedValue)

#define AssertFatalMsg1(_exp, _msg, a1) AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1)))
#define AssertFatalMsg2(_exp, _msg, a1, a2) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2)))
#define AssertFatalMsg3(_exp, _msg, a1, a2, a3) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3)))
#define AssertFatalMsg4(_exp, _msg, a1, a2, a3, a4) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4)))
#define AssertFatalMsg5(_exp, _msg, a1, a2, a3, a4, a5) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5)))
#define AssertFatalMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6)))
#define AssertFatalMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6)))
#define AssertFatalMsg7(_exp, _msg, a1, a2, a3, a4, a5, a6, a7) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6, a7)))
#define AssertFatalMsg8(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6, a7, a8)))
#define AssertFatalMsg9(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
  AssertFatalMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6, a7, a8, a9)))

#else  // DBGFLAG_ASSERTFATAL

#define AssertFatal(_exp) ((void)0)
#define AssertFatalOnce(_exp) ((void)0)
#define AssertFatalMsg(_exp, _msg) ((void)0)
#define AssertFatalMsgOnce(_exp, _msg) ((void)0)
#define AssertFatalFunc(_exp, _f) ((void)0)
#define AssertFatalEquals(_exp, _expectedValue) ((void)0)
#define AssertFatalFloatEquals(_exp, _expectedValue, _tol) ((void)0)
#define VerifyFatal(_exp) (_exp)
#define VerifyEqualsFatal(_exp, _expectedValue) (_exp)

#define AssertFatalMsg1(_exp, _msg, a1) ((void)0)
#define AssertFatalMsg2(_exp, _msg, a1, a2) ((void)0)
#define AssertFatalMsg3(_exp, _msg, a1, a2, a3) ((void)0)
#define AssertFatalMsg4(_exp, _msg, a1, a2, a3, a4) ((void)0)
#define AssertFatalMsg5(_exp, _msg, a1, a2, a3, a4, a5) ((void)0)
#define AssertFatalMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) ((void)0)
#define AssertFatalMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) ((void)0)
#define AssertFatalMsg7(_exp, _msg, a1, a2, a3, a4, a5, a6, a7) ((void)0)
#define AssertFatalMsg8(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8) ((void)0)
#define AssertFatalMsg9(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) ((void)0)

#endif  // DBGFLAG_ASSERTFATAL

// Assert macros
// Assert is used to detect an important but survivable error.
// It's only turned on when DBGFLAG_ASSERT is true.

#ifdef DBGFLAG_ASSERT

#define Assert(_exp) _AssertMsg(_exp, "Assertion Failed: " #_exp, ((void)0), false)
#define AssertMsg(_exp, _msg) _AssertMsg(_exp, _msg, ((void)0), false)
#define AssertOnce(_exp) _AssertMsgOnce(_exp, "Assertion Failed: " #_exp, false)
#define AssertMsgOnce(_exp, _msg) _AssertMsgOnce(_exp, _msg, false)
#define AssertFunc(_exp, _f) _AssertMsg(_exp, "Assertion Failed: " #_exp, _f, false)
#define AssertEquals(_exp, _expectedValue) \
  AssertMsg2((_exp) == (_expectedValue), "Expected %d but got %d!", (_expectedValue), (_exp))
#define AssertFloatEquals(_exp, _expectedValue, _tol)                              \
  AssertMsg2(fabs((_exp) - (_expectedValue)) <= (_tol), "Expected %f but got %f!", \
             (_expectedValue), (_exp))
#define Verify(_exp) Assert(_exp)
#define VerifyEquals(_exp, _expectedValue) AssertEquals(_exp, _expectedValue)

#define AssertMsg1(_exp, _msg, a1) AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1)))
#define AssertMsg2(_exp, _msg, a1, a2) AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2)))
#define AssertMsg3(_exp, _msg, a1, a2, a3) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3)))
#define AssertMsg4(_exp, _msg, a1, a2, a3, a4) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4)))
#define AssertMsg5(_exp, _msg, a1, a2, a3, a4, a5) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5)))
#define AssertMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6)))
#define AssertMsg7(_exp, _msg, a1, a2, a3, a4, a5, a6, a7) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6, a7)))
#define AssertMsg8(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6, a7, a8)))
#define AssertMsg9(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
  AssertMsg(_exp, (const ch *)(CDbgFmtMsg(_msg, a1, a2, a3, a4, a5, a6, a7, a8, a9)))

#else  // DBGFLAG_ASSERT

#define Assert(_exp) ((void)0)
#define AssertOnce(_exp) ((void)0)
#define AssertMsg(_exp, _msg) ((void)0)
#define AssertMsgOnce(_exp, _msg) ((void)0)
#define AssertFunc(_exp, _f) ((void)0)
#define AssertEquals(_exp, _expectedValue) ((void)0)
#define AssertFloatEquals(_exp, _expectedValue, _tol) ((void)0)
#define Verify(_exp) (_exp)
#define VerifyEquals(_exp, _expectedValue) (_exp)

#define AssertMsg1(_exp, _msg, a1) ((void)0)
#define AssertMsg2(_exp, _msg, a1, a2) ((void)0)
#define AssertMsg3(_exp, _msg, a1, a2, a3) ((void)0)
#define AssertMsg4(_exp, _msg, a1, a2, a3, a4) ((void)0)
#define AssertMsg5(_exp, _msg, a1, a2, a3, a4, a5) ((void)0)
#define AssertMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) ((void)0)
#define AssertMsg6(_exp, _msg, a1, a2, a3, a4, a5, a6) ((void)0)
#define AssertMsg7(_exp, _msg, a1, a2, a3, a4, a5, a6, a7) ((void)0)
#define AssertMsg8(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8) ((void)0)
#define AssertMsg9(_exp, _msg, a1, a2, a3, a4, a5, a6, a7, a8, a9) ((void)0)

#endif  // DBGFLAG_ASSERT

// These are always compiled in.
SOURCE_TIER0_API void Msg(const ch *pMsg, ...);
SOURCE_TIER0_API void DMsg(const ch *pGroupName, i32 level, const ch *pMsg, ...);

SOURCE_TIER0_API void Warning(const ch *pMsg, ...);
SOURCE_TIER0_API void DWarning(const ch *pGroupName, i32 level, const ch *pMsg, ...);

SOURCE_TIER0_API void Log(const ch *pMsg, ...);
SOURCE_TIER0_API void DLog(const ch *pGroupName, i32 level, const ch *pMsg, ...);

SOURCE_TIER0_API void Error(const ch *pMsg, ...);

// You can use this macro like a runtime assert macro.
// If the condition fails, then Error is called with the message. This macro is
// called like AssertMsg, where msg must be enclosed in parenthesis:
//
// ErrorIfNot( bCondition, ("a b c %d %d %d", 1, 2, 3) );
#define ErrorIfNot(condition, msg) \
  if ((condition))                 \
    ;                              \
  else {                           \
    Error msg;                     \
  }

/* A couple of super-common dynamic spew messages, here for convenience */
/* These looked at the "developer" group */
SOURCE_TIER0_API void DevMsg(i32 level, const ch *pMsg, ...);
SOURCE_TIER0_API void DevWarning(i32 level, const ch *pMsg, ...);
SOURCE_TIER0_API void DevLog(i32 level, const ch *pMsg, ...);

/* default level versions (level 1) */
SOURCE_TIER0_API_OVERLOAD void DevMsg(const ch *pMsg, ...);
SOURCE_TIER0_API_OVERLOAD void DevWarning(const ch *pMsg, ...);
SOURCE_TIER0_API_OVERLOAD void DevLog(const ch *pMsg, ...);

/* These looked at the "console" group */
SOURCE_TIER0_API void ConColorMsg(i32 level, const Color &clr, const ch *pMsg, ...);
SOURCE_TIER0_API void ConMsg(i32 level, const ch *pMsg, ...);
SOURCE_TIER0_API void ConWarning(i32 level, const ch *pMsg, ...);
SOURCE_TIER0_API void ConLog(i32 level, const ch *pMsg, ...);

/* default console version (level 1) */
SOURCE_TIER0_API_OVERLOAD void ConColorMsg(const Color &clr, const ch *pMsg, ...);
SOURCE_TIER0_API_OVERLOAD void ConMsg(const ch *pMsg, ...);
SOURCE_TIER0_API_OVERLOAD void ConWarning(const ch *pMsg, ...);
SOURCE_TIER0_API_OVERLOAD void ConLog(const ch *pMsg, ...);

/* developer console version (level 2) */
SOURCE_TIER0_API void ConDColorMsg(const Color &clr, const ch *pMsg, ...);
SOURCE_TIER0_API void ConDMsg(const ch *pMsg, ...);
SOURCE_TIER0_API void ConDWarning(const ch *pMsg, ...);
SOURCE_TIER0_API void ConDLog(const ch *pMsg, ...);

/* These looked at the "network" group */
SOURCE_TIER0_API void NetMsg(i32 level, const ch *pMsg, ...);
SOURCE_TIER0_API void NetWarning(i32 level, const ch *pMsg, ...);
SOURCE_TIER0_API void NetLog(i32 level, const ch *pMsg, ...);

void ValidateSpew(class CValidator &validator);

SOURCE_TIER0_API void COM_TimestampedLog(ch const *fmt, ...);

// Code macros, debugger interface.

#ifndef NDEBUG

#define DBG_CODE(_code) \
  if (0)                \
    ;                   \
  else {                \
    _code               \
  }
#define DBG_CODE_NOSCOPE(_code) _code
#define DBG_DCODE(_g, _l, _code) \
  if (IsSpewActive(_g, _l)) {    \
    _code                        \
  } else {                       \
  }
#define DBG_BREAK() DebuggerBreak() /* defined in platform.h */

#else  // NDEBUG

#define DBG_CODE(_code) ((void)0)
#define DBG_CODE_NOSCOPE(_code)
#define DBG_DCODE(_g, _l, _code) ((void)0)
#define DBG_BREAK() ((void)0)

#endif  // NDEBUG

#ifndef _RETAIL
class CScopeMsg {
 public:
  CScopeMsg(const ch *pszScope) {
    m_pszScope = pszScope;
    Msg("%s { ", pszScope);
  }
  ~CScopeMsg() { Msg("} %s", m_pszScope); }
  const ch *m_pszScope;
};
#define SCOPE_MSG(msg) CScopeMsg scopeMsg(msg)
#else
#define SCOPE_MSG(msg)
#endif

// Macro to assist in asserting constant invariants during compilation

#define COMPILE_TIME_ASSERT(pred) static_assert(pred, #pred)

#ifndef NDEBUG
#define ASSERT_INVARIANT(pred) \
  static void UNIQUE_ID() { COMPILE_TIME_ASSERT(pred); }
#else
#define ASSERT_INVARIANT(pred)
#endif

#ifndef NDEBUG
template <typename DEST_POINTER_TYPE, typename SOURCE_POINTER_TYPE>
inline DEST_POINTER_TYPE assert_cast(SOURCE_POINTER_TYPE *pSource) {
  Assert(static_cast<DEST_POINTER_TYPE>(pSource) == dynamic_cast<DEST_POINTER_TYPE>(pSource));
  return static_cast<DEST_POINTER_TYPE>(pSource);
}
#else
#define assert_cast static_cast
#endif

// Templates to assist in validating pointers:

// Have to use these stubs so we don't have to include windows.h here.
// These functions are obsolete and should not be used. Despite its names, it
// does not guarantee that the pointer is valid or that the memory pointed to is
// safe to use.
SOURCE_TIER0_API[[deprecated]] void _AssertValidReadPtr(void *ptr, i32 count = 1);
SOURCE_TIER0_API[[deprecated]] void _AssertValidWritePtr(void *ptr, i32 count = 1);
SOURCE_TIER0_API[[deprecated]] void _AssertValidReadWritePtr(void *ptr, i32 count = 1);
SOURCE_TIER0_API[[deprecated]] void AssertValidStringPtr(const ch *ptr, i32 maxchar = 0xFFFFFF);
template <class T>
[[deprecated]] inline void AssertValidReadPtr(T *ptr, i32 count = 1) {
  _AssertValidReadPtr((void *)ptr, count);
}
template <class T>
[[deprecated]] inline void AssertValidWritePtr(T *ptr, i32 count = 1) {
  _AssertValidWritePtr((void *)ptr, count);
}
template <class T>
[[deprecated]] inline void AssertValidReadWritePtr(T *ptr, i32 count = 1) {
  _AssertValidReadWritePtr((void *)ptr, count);
}

#define AssertValidThis() AssertValidReadWritePtr(this, sizeof(*this))

// Macro to protect functions that are not reentrant

#ifndef NDEBUG
class CReentryGuard {
 public:
  CReentryGuard(i32 *pSemaphore) : m_pSemaphore(pSemaphore) { ++(*m_pSemaphore); }

  ~CReentryGuard() { --(*m_pSemaphore); }

 private:
  i32 *m_pSemaphore;
};

#define ASSERT_NO_REENTRY()        \
  static i32 fSemaphore##__LINE__; \
  Assert(!fSemaphore##__LINE__);   \
  CReentryGuard ReentryGuard##__LINE__(&fSemaphore##__LINE__)
#else
#define ASSERT_NO_REENTRY()
#endif

// Purpose: Inline string formatter
#include "tier0/include/valve_off.h"
class CDbgFmtMsg {
 public:
  CDbgFmtMsg(const ch *format, ...) {
    va_list arg_ptr;

    va_start(arg_ptr, format);
    _vsnprintf_s(dbg_message_, ARRAYSIZE(dbg_message_) - 1, format, arg_ptr);
    va_end(arg_ptr);
  }

  operator const ch *() const { return dbg_message_; }

 private:
  ch dbg_message_[256];
};
#include "tier0/include/valve_on.h"

// Purpose: Embed debug info in each file.
#ifdef OS_WIN

#ifndef NDEBUG
#pragma comment(compiler)
#endif

#endif

// Purpose: Wrap around a variable to create a simple place to put a breakpoint
#ifndef NDEBUG

template <class Type>
class CDataWatcher {
 public:
  const Type &operator=(const Type &val) { return Set(val); }

  const Type &operator=(const CDataWatcher<Type> &val) { return Set(val.m_Value); }

  const Type &Set(const Type &val) {
    // Put your breakpoint here
    m_Value = val;
    return m_Value;
  }

  Type &GetForModify() { return m_Value; }

  const Type &operator+=(const Type &val) { return Set(m_Value + val); }

  const Type &operator-=(const Type &val) { return Set(m_Value - val); }

  const Type &operator/=(const Type &val) { return Set(m_Value / val); }

  const Type &operator*=(const Type &val) { return Set(m_Value * val); }

  const Type &operator^=(const Type &val) { return Set(m_Value ^ val); }

  const Type &operator|=(const Type &val) { return Set(m_Value | val); }

  const Type &operator++() { return (*this += 1); }

  Type operator--() { return (*this -= 1); }

  Type operator++(i32)  // postfix version..
  {
    Type val = m_Value;
    (*this += 1);
    return val;
  }

  Type operator--(i32)  // postfix version..
  {
    Type val = m_Value;
    (*this -= 1);
    return val;
  }

  // For some reason the compiler only generates type conversion warnings for
  // this operator when used like CNetworkVarBase<u8> = 0x1 (it warns
  // about converting from an i32 to an u8).
  template <class C>
  const Type &operator&=(C val) {
    return Set(m_Value & val);
  }

  operator const Type &() const { return m_Value; }

  const Type &Get() const { return m_Value; }

  const Type *operator->() const { return &m_Value; }

  Type m_Value;
};

#else

template <class Type>
class CDataWatcher {
 private:
  CDataWatcher();  // refuse to compile in non-debug builds
};

#endif

#endif  // SOURCE_TIER0_INCLUDE_DBG_H_
