// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "initmathlib.h"

#include "cmd.h"       // Cmd_*
#include "console.h"   // ConMsg
#include "gl_cvars.h"  // mat_overbright
#include "mathlib/mathlib.h"
#include "tier1/convar.h"  // ConVar define
#include "view.h"

#include "tier0/include/memdbgon.h"

namespace {
bool is_allow_3dnow{true};
bool is_allow_sse2{true};
}  // namespace

void InitMathlib() {
  MathLib_Init(2.2f,  // v_gamma.GetFloat()
               2.2f,  // v_texgamma.GetFloat()
               0.0f /*v_brightness.GetFloat() */,
               2.0f /*mat_overbright.GetInt() */, is_allow_3dnow, true,
               is_allow_sse2, true);
}

CON_COMMAND(r_sse2, "Enable/disable SSE2 code") {
  is_allow_sse2 = args.ArgC() == 1 || atoi(args[1]);

  InitMathlib();

  ConMsg("SSE2 code is %s.\n", MathLib_SSE2Enabled() ? "enabled" : "disabled");
}

CON_COMMAND(r_3dnow, "Enable/disable 3DNow code") {
  is_allow_3dnow = args.ArgC() == 1 || atoi(args[1]);

  InitMathlib();

  ConMsg("3DNow code is %s.\n",
         MathLib_3DNowEnabled() ? "enabled" : "disabled");
}
