// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: StudioMDL byteswapping functions.

#ifndef STUDIOBYTESWAP_H
#define STUDIOBYTESWAP_H

#include "base/include/base_types.h"
#include "tier1/byteswap.h"

struct studiohdr_t;
class IPhysicsCollision;

namespace StudioByteSwap {
using CompressFunc_t = bool (*)(const void *pInput, int inputSize,
                                void **pOutput, int *pOutputSize);

// void SetTargetBigEndian( bool bigEndian );
void ActivateByteSwapping(bool bActivate);
void SourceIsNative(bool bActivate);
void SetVerbose(bool bVerbose);
void SetCollisionInterface(IPhysicsCollision *pPhysicsCollision);

int ByteswapStudioFile(const char *pFilename, void *pOutBase,
                       const void *pFileBase, int fileSize, studiohdr_t *pHdr,
                       CompressFunc_t pCompressFunc = NULL);
int ByteswapPHY(void *pOutBase, const void *pFileBase, int fileSize);
int ByteswapANI(studiohdr_t *pHdr, void *pOutBase, const void *pFileBase,
                int filesize);
int ByteswapVVD(void *pOutBase, const void *pFileBase, int fileSize);
int ByteswapVTX(void *pOutBase, const void *pFileBase, int fileSize);
int ByteswapMDL(void *pOutBase, const void *pFileBase, int fileSize);
}  // namespace StudioByteSwap

#define BYTESWAP_ALIGNMENT_PADDING 4096

#endif  // STUDIOBYTESWAP_H
