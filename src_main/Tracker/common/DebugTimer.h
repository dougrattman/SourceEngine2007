// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DEBUGTIMER_H
#define DEBUGTIMER_H

// resets the timer
void Timer_Start();

// returns the time since Timer_Start() was called, in seconds
double Timer_End();

#endif  // DEBUGTIMER_H
