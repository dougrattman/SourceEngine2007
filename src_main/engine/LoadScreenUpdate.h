// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.

#ifndef LOAD_SCREEN_UPDATE_H
#define LOAD_SCREEN_UPDATE_H

#include "materialsystem/imaterialsystem.h"

// Activate, deactivate loader updates
void BeginLoadingUpdates(MaterialNonInteractiveMode_t mode);
void RefreshScreenIfNecessary();
void EndLoadingUpdates();

#endif /* LOAD_SCREEN_UPDATE_H */
