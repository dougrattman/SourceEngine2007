// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_SCOPED_SHOW_CURSOR_H_
#define BASE_INCLUDE_WINDOWS_SCOPED_SHOW_CURSOR_H_

#include <eh.h>

#include "base/include/check.h"
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Scoped show cursor.
class ScopedShowCursor {
 public:
  explicit ScopedShowCursor(bool is_show_cursor)
      : is_show_cursor_{is_show_cursor} {
    ::ShowCursor(is_show_cursor ? TRUE : FALSE);
  }

  ~ScopedShowCursor() { ::ShowCursor(is_show_cursor_ ? FALSE : TRUE); }

 private:
  const bool is_show_cursor_;

  ScopedShowCursor(const ScopedShowCursor& s) = delete;
  ScopedShowCursor& operator=(const ScopedShowCursor& s) = delete;
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_SCOPED_SHOW_CURSOR_H_
