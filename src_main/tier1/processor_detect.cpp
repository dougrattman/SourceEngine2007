// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Win32 dependant ASM code for CPU capability detection.

#include "tier1/processor_detect.h"

#if defined(WIN64)

// https://msdn.microsoft.com/en-us/library/ee418798(VS.85).aspx#Porting_to_64bit
// The x87, MMX, and 3DNow! instruction sets are deprecated in 64-bit modes.
// The instructions sets are still present for backward compatibility for 32-bit
// mode; however, to avoid compatibility issues in the future, their use in
// current and future projects is discouraged.

bool CheckMMXTechnology() { return false; }
bool CheckSSETechnology() { return true; }
bool CheckSSE2Technology() { return true; }
bool Check3DNowTechnology() { return false; }

#elif defined(_WIN32)

// https://technet.microsoft.com/en-us/library/dn482072.aspx
// PAE, NX, SSE2 support required for Windows 8 and later.
// https://msdn.microsoft.com/en-us/library/ee418798(VS.85).aspx#Porting_to_64bit
// x87, MMX and 3DNow! is obsolete, so to avoid compatibility issues in the
// future, their use in current and future projects is discouraged.

bool CheckMMXTechnology() { return false; }
bool CheckSSETechnology() { return true; }
bool CheckSSE2Technology() { return true; }
bool Check3DNowTechnology() { return false; }

#endif  // _WIN32
