// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VIEW_H
#define VIEW_H

class CViewSetup;
class CViewSetupV1;

void V_Init();
void V_Shutdown(void);
void ConvertViewSetup(const CViewSetup &setup, CViewSetupV1 &setupV1,
                      int nFlags);

#endif  // VIEW_H
