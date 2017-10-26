// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#if !defined(SCREEN_H)
#define SCREEN_H

void SCR_Init(void);
void SCR_Shutdown(void);

void SCR_UpdateScreen(void);
void SCR_CenterPrint(char *str);
void SCR_CenterStringOff(void);
void SCR_BeginLoadingPlaque(void);
void SCR_EndLoadingPlaque(void);
void SCR_FatalDiskError(void);

extern bool scr_disabled_for_loading;
extern int scr_nextdrawtick;

#endif  // SCREEN_H
