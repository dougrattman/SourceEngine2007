// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: A simple class for performing safe and in-expression sprintf-style
// string formatting.

#ifndef SOURCE_TIER1_FMTSTR_H_
#define SOURCE_TIER1_FMTSTR_H_

#include <cstdarg>
#include <cstdio>
#include <type_traits>
#include "tier0/platform.h"
#include "tier1/strtools.h"

// Purpose: String formatter with specified size
template <int SIZE_BUF>
class CFmtStrN {
 public:
  CFmtStrN() { m_szBuf[0] = 0; }

  // Standard C formatting
  template <typename... Args>
  CFmtStrN(const char *pszFormat, Args &&... args) {
    V_snprintf(m_szBuf, SIZE_BUF - 1, pszFormat, std::forward<Args>(args)...);
  }

  // Use this for pass-through formatting
  template <typename... Args>
  CFmtStrN(const char **ppszFormat, Args &&... args) {
    V_snprintf(m_szBuf, SIZE_BUF - 1, *ppszFormat, std::forward<Args>(args)...);
  }

  // Explicit reformat
  template <typename... Args>
  const char *sprintf(const char *pszFormat, Args &&... args) {
    V_snprintf(m_szBuf, SIZE_BUF - 1, pszFormat, std::forward<Args>(args)...);
    return m_szBuf;
  }

  // Use this for pass-through formatting
  template <typename... Args>
  void VSprintf(const char **ppszFormat, Args &&... args) {
    V_snprintf(m_szBuf, SIZE_BUF - 1, *ppszFormat, std::forward<Args>(args)...);
  }

  // Use for access
  operator const char *() const { return m_szBuf; }
  char *Access() { return m_szBuf; }

  void Clear() { m_szBuf[0] = 0; }

 private:
  char m_szBuf[SIZE_BUF];
};

// Purpose: Default-sized string formatter

#define FMTSTR_STD_LEN 256

typedef CFmtStrN<FMTSTR_STD_LEN> CFmtStr;

#endif  // SOURCE_TIER1_FMTSTR_H_
