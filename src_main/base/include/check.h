// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_CHECK_H_
#define BASE_INCLUDE_CHECK_H_

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "base/include/macros.h"

#ifndef NDEBUG
// Just use |code_|.
#define SOURCE_DBG_CODE_NOSCOPE(code_) code_
#else  // NDEBUG
// Skip |code_|.
#define SOURCE_DBG_CODE_NOSCOPE(code_) ((void)0)
#endif  // !NDEBUG

// Checks |condition| in DEBUG mode, if not, then print condition and
// |exit_code| to stderr and exit process with |exit_code|.  Process is
// terminated via std::exit call.
#define DCHECK(condition, exit_code)                   \
  assert(condition);                                   \
  SOURCE_DBG_CODE_NOSCOPE(if (!(condition)) {          \
    fprintf_s(stderr, "%s failed (%d).", (#condition), \
              implicit_cast<int>(exit_code));          \
    std::exit(exit_code);                              \
  })

// Checks |condition| in DEBUG mode, if not, then print condition and
// exit_code via |exit_code_lazy|() to stderr and exit process with
// |exit_code_lazy|().  Process is terminated via std::exit call.
#define DCHECK_LAZY_EXIT(condition, exit_code_lazy)    \
  assert(condition);                                   \
  SOURCE_DBG_CODE_NOSCOPE(if (!(condition)) {          \
    const auto exit_code = exit_code_lazy();           \
    fprintf_s(stderr, "%s failed (%d).", (#condition), \
              implicit_cast<int>(exit_code));          \
    std::exit(exit_code);                              \
  })

// Checks |condition|, if not, then print condition and |exit_code| to stderr
// and exit process with |exit_code|.  Process is terminated via std::exit call.
#define CHECK(condition, exit_code)                    \
  assert(condition);                                   \
  if (!(condition)) {                                  \
    fprintf_s(stderr, "%s failed (%d).", (#condition), \
              implicit_cast<int>(exit_code));          \
    std::exit(exit_code);                              \
  }

#endif  // BASE_INCLUDE_CHECK_H_
