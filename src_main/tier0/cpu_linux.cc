// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Determine CPU speed under linux.

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdio>
#include "base/include/base_types.h"

#define rdtsc(x) __asm__ __volatile__("rdtsc" : "=A"(x))

namespace {
struct TimeVal {
  inline double operator-(const TimeVal &left) {
    const u64 left_us =
        (u64)left.m_TimeVal.tv_sec * 1000000 + left.m_TimeVal.tv_usec;
    const u64 right_us = (u64)m_TimeVal.tv_sec * 1000000 + m_TimeVal.tv_usec;
    const u64 diff_us = left_us - right_us;
    return diff_us / 1000000;
  }

  timeval m_TimeVal;
};

// Compute the positive difference between two 64 bit numbers.
inline u64 diff(u64 v1, u64 v2) {
  const u64 d = v1 - v2;
  return d >= 0 ? d : -d;
}

u64 GetCpuFrequencyFromProcCpuInfo() {
  FILE *cpu_info_file{fopen("/proc/cpuinfo", "r")};
  if (!cpu_info_file) return 0;

  char cpu_info_line[1024], mhz_pattern[] = "cpu MHz";
  double mhz = 0;

  // Ignore all lines until we reach MHz information
  while (fgets(cpu_info_line, arraysize(cpu_info_line), cpu_info_file)) {
    if (strstr(cpu_info_line, mhz_pattern) != nullptr) {
      char *s;

      // Ignore all characters in line up to :
      for (s = cpu_info_line; *s && (*s != ':'); ++s)
        ;

      // Get MHz.
      if (*s && (sscanf(s + 1, "%lf", &mhz) == 1)) break;
    }
  }

  fclose(cpu_info_file);

  return static_cast<u64>(mhz * 1000000);
}
}  // namespace

u64 CalculateCPUFreq() {
  // Compute the period. Loop until we get 3 consecutive periods that
  // are the same to within a small error. The error is chosen
  // to be +/- 0.02% on a P-200.

  // over-ride by env var
  const char *cpu_mhz_env = getenv("CPU_MHZ");
  if (cpu_mhz_env) return 1000000 * atof(cpu_mhz_env);

  const u64 error = 40000;
  const int max_iterations = 600;
  int count;
  u64 period, period1 = error * 2, period2 = 0, period3 = 0;

  for (count = 0; count < max_iterations; count++) {
    TimeVal start_time, end_time;
    u64 start_tsc, end_tsc;
    gettimeofday(&start_time.m_TimeVal, 0);

    rdtsc(start_tsc);
    usleep(5000);  // sleep for 5 msec
    gettimeofday(&end_time.m_TimeVal, 0);
    rdtsc(end_tsc);

    period3 = (end_tsc - start_tsc) / (end_time - start_time);

    if (diff(period1, period2) <= error && diff(period2, period3) <= error &&
        diff(period1, period3) <= error)
      break;

    period1 = period2;
    period2 = period3;
  }

  if (count == max_iterations) return GetCpuFrequencyFromProcCpuInfo();

  // Set the period to the average period measured.
  period = (period1 + period2 + period3) / 3;

  // Some Pentiums have broken TSCs that increment very slowly or unevenly.
  return period >= 10000000 ? period : GetCpuFrequencyFromProcCpuInfo();
}
