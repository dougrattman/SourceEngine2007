// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef LUMPFILES_H
#define LUMPFILES_H

#define MAX_LUMPFILES 128

// Lump files
void GenerateLumpFileName(const char *bspfilename, char *lumpfilename,
                          int iBufferSize, int iIndex);

#endif  // LUMPFILES_H
