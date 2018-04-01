// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_CHECK_H_
#define BASE_INCLUDE_CHECK_H_

#include <cassert>
#include <cstdio>
#include <cstdlib>

#ifndef NDEBUG
#define SOURCE_DBG_CODE_NOSCOPE(code_) code_
#else  // NDEBUG
#define SOURCE_DBG_CODE_NOSCOPE(code_) ((void)0)
#endif  // !NDEBUG

#define DCHECK(condition, exit_code)                   \
  assert(condition);                                   \
  SOURCE_DBG_CODE_NOSCOPE(if (!(condition)) {          \
    fprintf_s(stderr, "%s failed (%d).", (#condition), \
              static_cast<int>(exit_code));            \
    std::_Exit(exit_code);                             \
  })

#define CHECK(condition, exit_code)                    \
  assert(condition);                                   \
  if (!(condition)) {                                  \
    fprintf_s(stderr, "%s failed (%d).", (#condition), \
              static_cast<int>(exit_code));            \
    std::_Exit(exit_code);                             \
  }

#endif  // BASE_INCLUDE_CHECK_H_
