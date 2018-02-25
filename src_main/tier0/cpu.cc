// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#ifdef OS_WIN
#include <intrin.h>
#include <array>
#elif defined(OS_POSIX)
#include <cstdio>
#endif

#include "tier0/include/fasttimer.h"

static inline bool cpuid(i32 function_id, i32& out_eax, i32& out_ebx,
                         i32& out_ecx, i32& out_edx) {
#ifdef OS_POSIX
  return false;
#else
  std::array<i32, 4> cpui;
  __cpuid(cpui.data(), function_id);

  out_eax = cpui[0];
  out_ebx = cpui[1];
  out_ecx = cpui[2];
  out_edx = cpui[3];

  return true;
#endif
}

bool CheckMMXTechnology() {
#ifdef OS_POSIX
  return true;
#else
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x800000) != 0;
#endif
}

bool CheckSSETechnology() {
#ifdef OS_POSIX
  return true;
#else
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) {
    return false;
  }

  return (edx & 0x2000000L) != 0;
#endif
}

bool CheckSSE2Technology() {
#ifdef OS_POSIX
  return false;
#else
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x04000000) != 0;
#endif
}

bool Check3DNowTechnology() {
#ifdef OS_POSIX
  return false;
#else
  i32 eax, unused;
  if (!cpuid(0x80000000, eax, unused, unused, unused)) return false;  //-V112

  if (eax > 0x80000000L) {  //-V112
    if (!cpuid(0x80000001, unused, unused, unused, eax)) return false;

    return (eax & 1 << 31) != 0;
  }
  return false;
#endif
}

bool CheckCMOVTechnology() {
#ifdef OS_POSIX
  return false;
#else
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & (1 << 15)) != 0;
#endif
}

bool CheckFCMOVTechnology() {
#ifdef OS_POSIX
  return false;
#else
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & (1 << 16)) != 0;
#endif
}

bool CheckRDTSCTechnology() {
#ifdef OS_POSIX
  return true;
#else
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x10) != 0;
#endif
}

// Return the Processor's vendor identification string, or "Generic_x86" if it
// doesn't exist on this CPU
const ch* GetProcessorVendorId() {
#ifdef OS_POSIX
  return "Generic_x86";
#else
  i32 unused, VendorIDRegisters[3];

  static ch VendorID[0x20];

  memset(VendorID, 0, sizeof(VendorID));
  if (!cpuid(0, unused, VendorIDRegisters[0], VendorIDRegisters[2],
             VendorIDRegisters[1])) {
    strcpy(VendorID, "Generic_x86");
  } else {
    memcpy(VendorID + 0, &(VendorIDRegisters[0]), sizeof(VendorIDRegisters[0]));
    memcpy(VendorID + 4, &(VendorIDRegisters[1]),
           sizeof(VendorIDRegisters[1]));  //-V112
    memcpy(VendorID + 8, &(VendorIDRegisters[2]), sizeof(VendorIDRegisters[2]));
  }

  return VendorID;
#endif
}

// Returns non-zero if Hyper-Threading Technology is supported on the processors
// and zero if not.  This does not mean that Hyper-Threading Technology is
// necessarily enabled.
static bool HTSupported() {
#if defined(OS_POSIX)
  // not entirely sure about the semantic of HT support, it being an intel name
  // are we asking about HW threads or HT?
  return true;
#else
  const u32 HT_BIT = 0x10000000;  // EDX[28] - Bit 28 set indicates
                                  // Hyper-Threading Technology is
                                  // supported in hardware.
  const u32 FAMILY_ID =
      0x0f00;  // EAX[11:8] - Bit 11 thru 8 contains family processor id
  const u32 EXT_FAMILY_ID = 0x0f00000;  // EAX[23:20] - Bit 23 thru 20
                                        // contains extended family
                                        // processor id
  const u32 PENTIUM4_ID = 0x0f00;       // Pentium 4 family processor id

  i32 unused, reg_eax = 0, reg_edx = 0, vendor_id[3] = {0, 0, 0};

  // verify cpuid instruction is supported
  if (!cpuid(0, unused, vendor_id[0], vendor_id[2], vendor_id[1]) ||
      !cpuid(1, reg_eax, unused, unused, reg_edx))
    return false;

  //  Check to see if this is a Pentium 4 or later processor
  if (((reg_eax & FAMILY_ID) == PENTIUM4_ID) || (reg_eax & EXT_FAMILY_ID))
    if (vendor_id[0] == 'uneG' && vendor_id[1] == 'Ieni' &&
        vendor_id[2] == 'letn')
      return (reg_edx & HT_BIT) !=
             0;  // Genuine Intel Processor with Hyper-Threading Technology

  return false;            // This is not a genuine Intel processor.
#endif
}

// Returns the number of logical processors per physical processors.
static u8 LogicalProcessorsPerPackage() {
  // EBX[23:16] indicate number of logical processors per package
  const u32 NUM_LOGICAL_BITS = 0x00FF0000;

  i32 unused, reg_ebx = 0;

  if (!HTSupported()) return 1;

  if (!cpuid(1, unused, reg_ebx, unused, unused)) return 1;

  return (u8)((reg_ebx & NUM_LOGICAL_BITS) >> 16);
}

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static i64 CalculateClockSpeed() {
#ifdef OS_WIN
  LARGE_INTEGER startCount, curCount;
  CCycleCount start, end;

  // Take 1/32 of a second for the measurement.
  u64 waitTime = Plat_PerformanceFrequency();
  i32 scale = 5;
  waitTime >>= scale;

  QueryPerformanceCounter(&startCount);
  start.Sample();
  do {
    QueryPerformanceCounter(&curCount);
  } while (curCount.QuadPart - startCount.QuadPart <
           static_cast<i64>(waitTime));
  end.Sample();

  return (end.m_Int64 - start.m_Int64) << scale;
#elif defined(OS_POSIX)
  u64 CalculateCPUFreq();  // from cpu_linux.cpp
  i64 freq = (int64)CalculateCPUFreq();
  if (freq == 0)  // couldn't calculate clock speed
  {
    Error("Unable to determine CPU Frequency\n");
  }
  return freq;
#endif
}

const CPUInformation& GetCPUInformation() {
  static CPUInformation pi;

  // Has the structure already been initialized and filled out?
  if (pi.m_Size == sizeof(pi)) return pi;

  // Redundant, but just in case the user somehow messes with the size.
  memset(&pi, 0x0, sizeof(pi));

  // Fill out the structure, and return it:
  pi.m_Size = sizeof(pi);

  // Grab the processor frequency:
  pi.m_Speed = CalculateClockSpeed();

  // Get the logical and physical processor counts:
  pi.m_nLogicalProcessors = LogicalProcessorsPerPackage();

#ifdef OS_WIN
  SYSTEM_INFO si = {};
  GetSystemInfo(&si);

  pi.m_nPhysicalProcessors =
      (u8)(si.dwNumberOfProcessors / pi.m_nLogicalProcessors);
  pi.m_nLogicalProcessors =
      (u8)(pi.m_nLogicalProcessors * pi.m_nPhysicalProcessors);

  // Make sure I always report at least one, when running WinXP with the /ONECPU
  // switch, it likes to report 0 processors for some reason.
  if (pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0) {
    pi.m_nPhysicalProcessors = 1;
    pi.m_nLogicalProcessors = 1;
  }
#elif defined(OS_POSIX)
  // TODO: poll /dev/cpuinfo when we have some benefits from multithreading
  pi.m_nPhysicalProcessors = 1;
  pi.m_nLogicalProcessors = 1;
#endif

  // Determine Processor Features:
  pi.m_bRDTSC = CheckRDTSCTechnology();
  pi.m_bCMOV = CheckCMOVTechnology();
  pi.m_bFCMOV = CheckFCMOVTechnology();
  pi.m_bMMX = CheckMMXTechnology();
  pi.m_bSSE = CheckSSETechnology();
  pi.m_bSSE2 = CheckSSE2Technology();
  pi.m_b3DNow = Check3DNowTechnology();
  pi.m_szProcessorID = (ch*)GetProcessorVendorId();
  pi.m_bHT = HTSupported();

  return pi;
}
