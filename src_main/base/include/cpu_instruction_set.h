// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_CPU_INSTRUCTION_SET_H_
#define BASE_INCLUDE_CPU_INSTRUCTION_SET_H_

#include "build/include/build_config.h"

#ifdef COMPILER_MSVC
#include <intrin.h>
#endif

#include <array>
#include <bitset>

#include "base/include/base_types.h"

namespace source {
// CPUID for CLANG / GCC / MSVC.
inline std::array<i32, 4> cpuid(i32 function_id) {
  std::array<i32, 4> cpui;

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#ifdef ARCH_CPU_X86_64
  asm volatile(
      "movq\t%%rbx, %%rsi\n\t"
      "cpuid\n\t"
      "xchgq\t%%rbx, %%rsi\n\t"
      : "=a"(cpui[0]), "=S"(cpui[1]), "=c"(cpui[2]), "=d"(cpui[3])
      : "a"(function_id));
  return cpui;
#elif defined(ARCH_CPU_X86)
  asm volatile(
      "movl\t%%ebx, %%esi\n\t"
      "cpuid\n\t"
      "xchgl\t%%ebx, %%esi\n\t"
      : "=a"(cpui[0]), "=S"(cpui[1]), "=c"(cpui[2]), "=d"(cpui[3])
      : "a"(function_id));
  return cpui;
#else
#error Please add cpuid support in tier0/system_info.cc
  return cpui;
#endif  // ARCH_CPU_X86_64
#elif defined(COMPILER_MSVC)
  __cpuid(cpui.data(), function_id);
  return cpui;
#endif
}

// CPUIDEX for CLANG / GCC / MSVC.
inline std::array<i32, 4> cpuidex(i32 function_id, i32 subfunction_id) {
  std::array<i32, 4> cpui;

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#ifdef ARCH_CPU_X86_64
  // The __cpuidex intrinsic sets the value of the ECX register to
  // subfunction_id before it generates the cpuid instruction.
  asm volatile(
      "movq\t%%rbx, %%rsi\n\t"
      "cpuid\n\t"
      "xchgq\t%%rbx, %%rsi\n\t"
      : "=a"(cpui[0]), "=S"(cpui[1]), "=c"(cpui[2]), "=d"(cpui[3])
      : "a"(function_id), "c"(subfunction_id));
  return cpui;
#elif defined(ARCH_CPU_X86)
  asm volatile(
      "movl\t%%ebx, %%esi\n\t"
      "cpuid\n\t"
      "xchgl\t%%ebx, %%esi\n\t"
      : "=a"(cpui[0]), "=S"(cpui[1]), "=c"(cpui[2]), "=d"(cpui[3])
      : "a"(function_id), "c"(subfunction_id));
  return cpui;
#else
#error Please add cpuid support in tier0/system_info.cc
  return cpui;
#endif  // ARCH_CPU_X86_64
#elif defined(COMPILER_MSVC)
  __cpuidex(cpui.data(), function_id, subfunction_id);
  return cpui;
#endif
}

// Cpu instruction set accessor wrapper. Not all instructions / features are
// included, add if necessary.  See "Intel® 64 and IA-32 Architectures Software
// Developer’s Manual Volume 2 (2A, 2B, 2C & 2D): Instruction Set Reference,
// A-Z."
// https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-instruction-set-reference-manual-325383.pdf
// See "AMD64 Architecture Programmer’s Manual, Volume 3: General-Purpose and
// System Instructions" https://support.amd.com/TechDocs/24594.pdf
class CpuInstructionSet {
 public:
  static str Vendor() { return CpuIs().vendor_; }
  static str Brand() { return CpuIs().brand_; }

  static bool HasSse3() noexcept { return CpuIs().f_1_ecx_[0]; }
  static bool HasPclmulqdq() noexcept { return CpuIs().f_1_ecx_[1]; }
  static bool HasDtes() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[2];
  }
  static bool HasMonitor() noexcept { return CpuIs().f_1_ecx_[3]; }
  static bool HasDsCpl() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[4];
  }
  static bool HasVmx() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[5];
  }
  static bool HasSmx() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[6];
  }
  static bool HasEist() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[7];
  }
  static bool HasTm2() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[8];
  }
  static bool HasSsse3() noexcept { return CpuIs().f_1_ecx_[9]; }
  static bool HasCnxtId() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[10];
  }
  static bool HasSdbg() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[11];
  }
  static bool HasFma() noexcept { return CpuIs().f_1_ecx_[12]; }
  static bool HasCmpxchg16b() noexcept { return CpuIs().f_1_ecx_[13]; }
  static bool HasXtpr() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[14];
  }
  static bool HasPdcm() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[15];
  }
  static bool HasPcid() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[17];
  }
  static bool HasDca() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[18];
  }
  static bool HasSse4_1() noexcept { return CpuIs().f_1_ecx_[19]; }
  static bool HasSse4_2() noexcept { return CpuIs().f_1_ecx_[20]; }
  static bool HasX2apic() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[21];
  }
  static bool HasMovbe() noexcept { return CpuIs().f_1_ecx_[22]; }
  static bool HasPopcnt() noexcept { return CpuIs().f_1_ecx_[23]; }
  static bool HasTscDeadline() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_1_ecx_[24];
  }
  static bool HasAes() noexcept { return CpuIs().f_1_ecx_[25]; }
  static bool HasXsave() noexcept { return CpuIs().f_1_ecx_[26]; }
  static bool HasOsXsave() noexcept { return CpuIs().f_1_ecx_[27]; }
  static bool HasAvx() noexcept { return CpuIs().f_1_ecx_[28]; }
  static bool HasF16c() noexcept { return CpuIs().f_1_ecx_[29]; }
  static bool HasRdrand() noexcept { return CpuIs().f_1_ecx_[30]; }

  static bool HasFpu() noexcept { return CpuIs().f_1_edx_[0]; }
  static bool HasVme() noexcept { return CpuIs().f_1_edx_[1]; }
  static bool HasDe() noexcept { return CpuIs().f_1_edx_[2]; }
  static bool HasPse() noexcept { return CpuIs().f_1_edx_[3]; }
  static bool HasRdtsc() noexcept { return CpuIs().f_1_edx_[4]; }
  static bool HasMsr() noexcept { return CpuIs().f_1_edx_[5]; }
  static bool HasPae() noexcept { return CpuIs().f_1_edx_[6]; }
  static bool HasMce() noexcept { return CpuIs().f_1_edx_[7]; }
  static bool HasCmpxchg8b() noexcept { return CpuIs().f_1_edx_[8]; }
  static bool HasApic() noexcept { return CpuIs().f_1_edx_[9]; }
  static bool HasSep() noexcept { return CpuIs().f_1_edx_[11]; }
  static bool HasMtrr() noexcept { return CpuIs().f_1_edx_[12]; }
  static bool HasCmov() noexcept { return CpuIs().f_1_edx_[15]; }
  static bool HasFcmov() noexcept { return HasFpu() && CpuIs().f_1_edx_[15]; }
  static bool HasClfsh() noexcept { return CpuIs().f_1_edx_[19]; }
  static bool HasMmx() noexcept { return CpuIs().f_1_edx_[23]; }
  static bool HasFxsr() noexcept { return CpuIs().f_1_edx_[24]; }
  static bool HasSse() noexcept { return CpuIs().f_1_edx_[25]; }
  static bool HasSse2() noexcept { return CpuIs().f_1_edx_[26]; }

  static bool HasFsgbase() noexcept { return CpuIs().f_7_ebx_[0]; }
  static bool HasBmi1() noexcept { return CpuIs().f_7_ebx_[3]; }
  static bool HasHle() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_7_ebx_[4];
  }
  static bool HasAvx2() noexcept { return CpuIs().f_7_ebx_[5]; }
  static bool HasBmi2() noexcept { return CpuIs().f_7_ebx_[8]; }
  static bool HasErms() noexcept { return CpuIs().f_7_ebx_[9]; }
  static bool HasInvpcid() noexcept { return CpuIs().f_7_ebx_[10]; }
  static bool HasRtm() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_7_ebx_[11];
  }
  static bool HasAvx512f() noexcept { return CpuIs().f_7_ebx_[16]; }
  static bool HasRdseed() noexcept { return CpuIs().f_7_ebx_[18]; }
  static bool HasAdx() noexcept { return CpuIs().f_7_ebx_[19]; }
  static bool HasAvx512pf() noexcept { return CpuIs().f_7_ebx_[26]; }
  static bool HasAvx512er() noexcept { return CpuIs().f_7_ebx_[27]; }
  static bool HasAvx512cd() noexcept { return CpuIs().f_7_ebx_[28]; }
  static bool HasSha() noexcept { return CpuIs().f_7_ebx_[29]; }

  static bool HasPrefetchwt1() noexcept { return CpuIs().f_7_ecx_[0]; }

  static bool HasInvariantTsc() noexcept { return CpuIs().f_7_edx_[8]; }

  static bool HasLahfSahf() noexcept { return CpuIs().f_81_ecx_[0]; }
  static bool HasSvm() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[2];
  }
  // META ExtApicSpace: extended APIC space. This bit indicates the presence of
  // extended APIC register space starting at offset 400h from the “APIC Base
  // Address Register, ” as specified in the BKDG.
  static bool HasExtApicSpace() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[3];
  }
  static bool HasLzcnt() noexcept {
    return CpuIs().is_intel_ && CpuIs().f_81_ecx_[5];
  }
  static bool HasAbm() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[5];
  }
  static bool HasSse4a() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[6];
  }
  // META MisAlignSse: misaligned SSE mode.
  static bool HasMisAlignSse() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[7];
  }
  static bool Has3dNowPrefetch() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[8];
  }
  // META IBS: instruction based sampling.
  static bool HasIbs() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[10];
  }
  static bool HasXop() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[11];
  }
  // META WDT: watchdog timer support.
  static bool HasWdt() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[13];
  }
  // META LWP: lightweight profiling support.
  static bool HasLwp() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[15];
  }
  static bool HasFma4() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[16];
  }
  static bool HasTbm() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_ecx_[21];
  }

  static bool HasSyscall() noexcept { return CpuIs().f_81_edx_[11]; }
  // META NX: has no-execute page protection.
  static bool HasNx() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_edx_[20];
  }
  static bool HasMmxExt() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_edx_[22];
  }
  // FFXSR: FXSAVE and FXRSTOR instruction optimizations.
  static bool HasFfxsr() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_edx_[25];
  }
  static bool HasRdtscp() noexcept { return CpuIs().f_81_edx_[27]; }

  // META LM: Long Mode, or Intel 64.
  static bool HasLM() noexcept { return CpuIs().f_81_edx_[29]; }
  static bool Has3dNowExt() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_edx_[30];
  }
  static bool Has3dNow() noexcept {
    return CpuIs().is_amd_ && CpuIs().f_81_edx_[31];
  }

 private:
  // Cpu instruction set quering.
  class CpuInstructionSet_Internal {
   public:
    CpuInstructionSet_Internal() noexcept
        : func_ids_count_{0},
          ext_func_ids_count_{0},
          is_intel_{false},
          is_amd_{false},
          f_1_ecx_{0},
          f_1_edx_{0},
          f_7_ebx_{0},
          f_7_ecx_{0},
          f_7_edx_{0},
          f_81_ecx_{0},
          f_81_edx_{0},
          data_{},
          ext_data_{} {
      // Calling cpuid with 0x0 as the function_id argument gets the number
      // of the highest valid function ID.
      std::array<i32, 4> cpui = cpuid(0);
      func_ids_count_ = cpui[0];

      for (i32 i = 0; i <= func_ids_count_; ++i) {
        data_.emplace_back(cpuidex(i, 0));
      }

      // Capture vendor string.
      ch vendor[0x20];
      std::memset(vendor, 0, sizeof(vendor));
      *reinterpret_cast<i32*>(vendor) = data_[0][1];
      *reinterpret_cast<i32*>(vendor + 4) = data_[0][3];  //-V112
      *reinterpret_cast<i32*>(vendor + 8) = data_[0][2];
      vendor_ = vendor;
      if (vendor_ == "GenuineIntel") {
        is_intel_ = true;
      } else if (vendor_ == "AuthenticAMD") {
        is_amd_ = true;
      }

      // Load bitset with flags for function 0x00000001.
      if (func_ids_count_ >= 1) {
        f_1_ecx_ = data_[1][2];
        f_1_edx_ = data_[1][3];
      }

      // Load bitset with flags for function 0x00000007.
      if (func_ids_count_ >= 7) {
        f_7_ebx_ = data_[7][1];
        f_7_ecx_ = data_[7][2];
        f_7_edx_ = data_[7][3];
      }

      // Calling cpuid with 0x80000000 as the function_id argument gets the
      // number of the highest valid extended ID.
      cpui = cpuid(0x80000000);  //-V112
      ext_func_ids_count_ = cpui[0];

      ch brand[sizeof(cpui) * 3];
      std::memset(brand, 0, sizeof(brand));

      for (i32 i = 0x80000000; i <= ext_func_ids_count_; ++i) {  //-V112
        ext_data_.emplace_back(cpuidex(i, 0));
      }

      // Load bitset with flags for function 0x80000001.
      if (ext_func_ids_count_ >= 0x80000001) {
        f_81_ecx_ = ext_data_[1][2];
        f_81_edx_ = ext_data_[1][3];
      }

      // Interpret cpu brand string if reported.
      if (ext_func_ids_count_ >= 0x80000004) {
        std::memcpy(brand, ext_data_[2].data(), sizeof(cpui));
        std::memcpy(brand + sizeof(cpui), ext_data_[3].data(), sizeof(cpui));
        std::memcpy(brand + sizeof(cpui) * 2, ext_data_[4].data(),
                    sizeof(cpui));
        brand_ = brand;
      }
    };

    i32 func_ids_count_, ext_func_ids_count_;
    str vendor_, brand_;
    bool is_intel_, is_amd_;

    std::bitset<32> f_1_ecx_;  //-V112
    std::bitset<32> f_1_edx_;
    std::bitset<32> f_7_ebx_;
    std::bitset<32> f_7_ecx_;
    std::bitset<32> f_7_edx_;
    std::bitset<32> f_81_ecx_;
    std::bitset<32> f_81_edx_;

    vec<std::array<i32, 4>> data_;
    vec<std::array<i32, 4>> ext_data_;
  };

  static const CpuInstructionSet_Internal& CpuIs() noexcept {
    static const CpuInstructionSet_Internal cpu_is;
    return cpu_is;
  }
};
}  // namespace source

#endif  // BASE_INCLUDE_CPU_INSTRUCTION_SET_H_
