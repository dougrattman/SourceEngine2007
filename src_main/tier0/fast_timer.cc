// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/fasttimer.h"

i64 g_ClockSpeed;
unsigned long g_dwClockSpeed;

f64 g_ClockSpeedMicrosecondsMultiplier;
f64 g_ClockSpeedMillisecondsMultiplier;
f64 g_ClockSpeedSecondsMultiplier;

// Constructor init the clock speed.
CClockSpeedInit g_ClockSpeedInit;
