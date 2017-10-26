// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#if !defined(MEM_FGETS_H)
#define MEM_FGETS_H

char *memfgets(unsigned char *pMemFile, int fileSize, int *pFilePos,
               char *pBuffer, int bufferSize);

#endif  // MEM_FGETS_H
