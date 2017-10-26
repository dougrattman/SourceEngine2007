// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#if defined(_WIN32)
#include <intrin.h>
#include <array>
#include "winlite.h"
#elif defined(_LINUX)
#include <cstdio>
#endif

#include "tier0/platform.h"

static inline bool cpuid(int function_id, int& out_eax, int& out_ebx,
                         int& out_ecx, int& out_edx) {
#ifdef _LINUX
  return false;
#else
  std::array<int, 4> cpui;
  __cpuid(cpui.data(), function_id);

  out_eax = cpui[0];
  out_ebx = cpui[1];
  out_ecx = cpui[2];
  out_edx = cpui[3];

  return true;
#endif
}

bool CheckMMXTechnology() {
#ifdef _LINUX
  return true;
#else
  int eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x800000) != 0;
#endif
}

bool CheckSSETechnology() {
#ifdef _LINUX
  return true;
#else
  int eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) {
    return false;
  }

  return (edx & 0x2000000L) != 0;
#endif
}

bool CheckSSE2Technology() {
#ifdef _LINUX
  return false;
#else
  int eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x04000000) != 0;
#endif
}

bool Check3DNowTechnology() {
#ifdef _LINUX
  return false;
#else
  int eax, unused;
  if (!cpuid(0x80000000, eax, unused, unused, unused)) return false;  //-V112

  if (eax > 0x80000000L) {  //-V112
    if (!cpuid(0x80000001, unused, unused, unused, eax)) return false;

    return (eax & 1 << 31) != 0;
  }
  return false;
#endif
}

bool CheckCMOVTechnology() {
#ifdef _LINUX
  return false;
#else
  int eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & (1 << 15)) != 0;
#endif
}

bool CheckFCMOVTechnology() {
#ifdef _LINUX
  return false;
#else
  int eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & (1 << 16)) != 0;
#endif
}

bool CheckRDTSCTechnology() {
#ifdef _LINUX
  return true;
#else
  int eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x10) != 0;
#endif
}

// Return the Processor's vendor identification string, or "Generic_x86" if it
// doesn't exist on this CPU
const char* GetProcessorVendorId() {
#ifdef _LINUX
  return "Generic_x86";
#else
  int unused, VendorIDRegisters[3];

  static char VendorID[0x20];

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
#if defined(_LINUX)
  // not entirely sure about the semantic of HT support, it being an intel name
  // are we asking about HW threads or HT?
  return true;
#else
  const unsigned int HT_BIT = 0x10000000;  // EDX[28] - Bit 28 set indicates
                                           // Hyper-Threading Technology is
                                           // supported in hardware.
  const unsigned int FAMILY_ID =
      0x0f00;  // EAX[11:8] - Bit 11 thru 8 contains family processor id
  const unsigned int EXT_FAMILY_ID = 0x0f00000;  // EAX[23:20] - Bit 23 thru 20
                                                 // contains extended family
                                                 // processor id
  const unsigned int PENTIUM4_ID = 0x0f00;  // Pentium 4 family processor id

  int unused, reg_eax = 0, reg_edx = 0, vendor_id[3] = {0, 0, 0};

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

  return false;                 // This is not a genuine Intel processor.
#endif
}

// Returns the number of logical processors per physical processors.
static uint8_t LogicalProcessorsPerPackage() {
  // EBX[23:16] indicate number of logical processors per package
  const unsigned NUM_LOGICAL_BITS = 0x00FF0000;

  int unused, reg_ebx = 0;

  if (!HTSupported()) return 1;

  if (!cpuid(1, unused, reg_ebx, unused, unused)) return 1;

  return (uint8_t)((reg_ebx & NUM_LOGICAL_BITS) >> 16);
}

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64_t CalculateClockSpeed() {
#if defined(_WIN32)
  LARGE_INTEGER startCount, curCount;
  CCycleCount start, end;

  // Take 1/32 of a second for the measurement.
  uint64_t waitTime = Plat_PerformanceFrequency();
  int scale = 5;
  waitTime >>= scale;

  QueryPerformanceCounter(&startCount);
  start.Sample();
  do {
    QueryPerformanceCounter(&curCount);
  } while (curCount.QuadPart - startCount.QuadPart <
           static_cast<int64_t>(waitTime));
  end.Sample();

  return (end.m_Int64 - start.m_Int64) << scale;
#elif defined(_LINUX)
  uint64_t CalculateCPUFreq();  // from cpu_linux.cpp
  int64_t freq = (int64)CalculateCPUFreq();
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

#if defined(_WIN32)
  SYSTEM_INFO si;
  ZeroMemory(&si, sizeof(si));

  GetSystemInfo(&si);

  pi.m_nPhysicalProcessors =
      (unsigned char)(si.dwNumberOfProcessors / pi.m_nLogicalProcessors);
  pi.m_nLogicalProcessors =
      (unsigned char)(pi.m_nLogicalProcessors * pi.m_nPhysicalProcessors);

  // Make sure I always report at least one, when running WinXP with the /ONECPU
  // switch, it likes to report 0 processors for some reason.
  if (pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0) {
    pi.m_nPhysicalProcessors = 1;
    pi.m_nLogicalProcessors = 1;
  }
#elif defined(_LINUX)
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
  pi.m_szProcessorID = (char*)GetProcessorVendorId();
  pi.m_bHT = HTSupported();

  return pi;
}
