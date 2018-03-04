// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/platform.h"

#include <array>

#include "base/include/windows/windows_light.h"
#include "tier0/include/fasttimer.h"

#ifdef COMPILER_MSVC
#include <intrin.h>
#endif

namespace {
inline bool cpuid(i32 function_id, i32& out_eax, i32& out_ebx, i32& out_ecx,
                  i32& out_edx) {
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#ifdef ARCH_CPU_X86_64
  __asm__(
      "movq\t%%rbx, %%rsi\n\t"
      "cpuid\n\t"
      "xchgq\t%%rbx, %%rsi\n\t"
      : "=a"(out_eax), "=S"(out_ebx), "=c"(out_ecx), "=d"(out_edx)
      : "a"(function_id));
  return true;
#elif defined(ARCH_CPU_X86)
  __asm__(
      "movl\t%%ebx, %%esi\n\t"
      "cpuid\n\t"
      "xchgl\t%%ebx, %%esi\n\t"
      : "=a"(out_eax), "=S"(out_ebx), "=c"(out_ecx), "=d"(out_edx)
      : "a"(function_id));
  return true;
#else
#error Please, define cpuid for your platform in tier0/cpu.cc
  return false;
#endif
#elif defined(COMPILER_MSVC)
  std::array<i32, 4> cpui;
  __cpuid(cpui.data(), function_id);

  out_eax = cpui[0];
  out_ebx = cpui[1];
  out_ecx = cpui[2];
  out_edx = cpui[3];

  return true;
#endif
}

bool HasMmx() {
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x800000) != 0;
}

bool HasSse() {
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x2000000) != 0;
}

bool HasSse2() {
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x04000000) != 0;
}

bool Has3dNow() {
  i32 eax, unused;
  if (!cpuid(0x80000000, eax, unused, unused, unused)) return false;  //-V112

  if (eax > 0x80000000) {  //-V112
    if (!cpuid(0x80000001, unused, unused, unused, eax)) return false;

    return (eax & 1 << 31) != 0;
  }

  return false;
}

bool HasCmov() {
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & (1 << 15)) != 0;
}

bool HasFcmov() {
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & (1 << 16)) != 0;
}

bool HasRdtsc() {
  i32 eax, ebx, edx, unused;
  if (!cpuid(1, eax, ebx, unused, edx)) return false;

  return (edx & 0x10) != 0;
}

// Return the Processor's vendor identification string, or "Generic_x86" if it
// doesn't exist on this CPU
ch* GetCpuVendorId() {
  i32 unused, registers[3];
  static ch vendor_id[0x20];

  memset(vendor_id, 0, sizeof(vendor_id));
  if (!cpuid(0, unused, registers[0], registers[2], registers[1])) {
#ifdef ARCH_CPU_X86
    strcpy(vendor_id, "Generic_x86");
#elif defined(ARCH_CPU_X86_64)
    strcpy(vendor_id, "Generic_x86_64");
#else
#error Please, define yout cpu architecture in tier0/cpu.cc
#endif
  } else {
    memcpy(vendor_id + 0, &(registers[0]), sizeof(registers[0]));
    memcpy(vendor_id + sizeof(registers[0]), &(registers[1]),
           sizeof(registers[1]));  //-V112
    memcpy(vendor_id + sizeof(registers[0]) + sizeof(registers[1]),
           &(registers[2]), sizeof(registers[2]));
  }

  return vendor_id;
}

// Returns non-zero if Hyper-Threading Technology is supported on the processors
// and zero if not. This does not mean that Hyper-Threading Technology is
// necessarily enabled.
bool HasHt() {
  // EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in
  // hardware.
  constexpr u32 kHtBit{0x10000000};
  // EAX[11:8] - Bit 11 thru 8 contains family processor id.
  constexpr u32 kFamliyId{0x0F00};
  // EAX[23:20] - Bit 23 thru 20 contains extended family processor id.
  constexpr u32 kExtFamilyId{0x0f00000};
  // Pentium 4 family processor id.
  constexpr u32 kPentium4Id{0x0F00};

  i32 unused, reg_eax = 0, reg_edx = 0, vendor_id[3] = {0, 0, 0};

  // verify cpuid instruction is supported.
  if (!cpuid(0, unused, vendor_id[0], vendor_id[2], vendor_id[1]) ||
      !cpuid(1, reg_eax, unused, unused, reg_edx))
    return false;

  // Check to see if this is a Pentium 4 or later processor.
  if (((reg_eax & kFamliyId) == kPentium4Id) || (reg_eax & kExtFamilyId))
    if (vendor_id[0] == 'uneG' && vendor_id[1] == 'Ieni' &&
        vendor_id[2] == 'letn')
      // Genuine Intel Processor with Hyper-Threading Technology.
      return (reg_edx & kHtBit) != 0;

  // This is not a genuine Intel processor.
  return false;
}

// Returns the number of logical processors per physical processors.
u8 LogicalProcessorsPerPackage() {
  // EBX[23:16] indicate number of logical processors per package.
  constexpr u32 kNumLogicalBits{0x00FF0000};

  i32 unused, reg_ebx = 0;

  if (!HasHt()) return 1;
  if (!cpuid(1, unused, reg_ebx, unused, unused)) return 1;

  return (u8)((reg_ebx & kNumLogicalBits) >> 16);
}

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
i64 CalculateClockSpeed() {
#ifdef OS_WIN
  LARGE_INTEGER start_count, end_count;
  CCycleCount start_cycle, end_cycle;

  i32 scale = 5;
  // Take 1/32 of a second for the measurement.
  u64 waitTime = Plat_PerformanceFrequency() >> scale;

  QueryPerformanceCounter(&start_count);
  start_cycle.Sample();
  do {
    QueryPerformanceCounter(&end_count);
  } while (end_count.QuadPart - start_count.QuadPart <
           static_cast<i64>(waitTime));
  end_cycle.Sample();

  return (end_cycle.m_Int64 - start_cycle.m_Int64) << scale;
#elif defined(OS_POSIX)
  u64 CalculateCPUFreq();  // from cpu_linux.cc
  i64 freq = (i64)CalculateCPUFreq();
  if (freq == 0) {  // couldn't calculate clock speed
    Error("Unable to determine CPU Frequency\n");
  }
  return freq;
#endif
}
}  // namespace

const CPUInformation& GetCPUInformation() {
  static CPUInformation pi;

  // Has the structure already been initialized and filled out?
  if (pi.m_Size == sizeof(pi)) return pi;

  // Redundant, but just in case the user somehow messes with the size.
  memset(&pi, 0x00, sizeof(pi));

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
  // Assume at least 2 logical processors.
  pi.m_nPhysicalProcessors = 1;
  pi.m_nLogicalProcessors = 2;
#endif

  // Determine Processor Features:
  pi.m_bRDTSC = HasRdtsc();
  pi.m_bCMOV = HasCmov();
  pi.m_bFCMOV = HasFcmov();
  pi.m_bMMX = HasMmx();
  pi.m_bSSE = HasSse();
  pi.m_bSSE2 = HasSse2();
  pi.m_b3DNow = Has3dNow();
  pi.m_szProcessorID = GetCpuVendorId();
  pi.m_bHT = HasHt();

  return pi;
}
