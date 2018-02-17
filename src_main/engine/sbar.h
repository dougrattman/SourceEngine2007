// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SBAR_H
#define SBAR_H

// the status bar is only redrawn if something has changed, but if anything
// does, the entire thing will be redrawn for the next vid.numpages frames.
void Sbar_Draw();
// called every frame by screen

#endif  // SBAR_H
