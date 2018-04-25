// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef UTIL_H
#define UTIL_H

// Purpose: Utility function for the server browser
class CUtil {
 public:
  const char *InfoGetValue(const char *s, const char *key);
  const char *GetString(const char *stringName);
};

extern CUtil *util;

#endif  // UTIL_H
