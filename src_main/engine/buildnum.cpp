// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "quakedef.h"
#include "tier1/strtools.h"

#include "tier0/memdbgon.h"

// char *date = "Nov 07 1998"; // "Oct 24 1996";
const char *now_date = __DATE__;

const char *short_month_names[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
char month_day_couns[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int ComputeBuildNumber() {
  int m = 0, d = 0;

  for (m = 0; m < 11; m++) {
    if (Q_strncasecmp(&now_date[0], short_month_names[m], 3) == 0) break;
    d += month_day_couns[m];
  }

  d += atoi(&now_date[4]) - 1;

  const int y = atoi(&now_date[7]) - 1900;
  int build_number = d + (int)((y - 1) * 365.25);

  if (((y % 4) == 0) && m > 1) {
    build_number += 1;
  }

  // m_nBuildNumber -= 34995; // Oct 24 1996
  build_number -= 35739;  // Nov 7 1998 (HL1 Gold Date)

  return build_number;
}

// Purpose: Only compute build number the first time we run the app.
int build_number() {
  static int build_number = ComputeBuildNumber();
  return build_number;
}
