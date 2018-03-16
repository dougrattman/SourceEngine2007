// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_VERTEXDECL_H_
#define MATERIALSYSTEM_SHADERAPIDX9_VERTEXDECL_H_

#include "locald3dtypes.h"
#include "materialsystem/IMaterial.h"

// Gets the declspec associated with a vertex format
IDirect3DVertexDeclaration9 *FindOrCreateVertexDecl(VertexFormat_t fmt,
                                                    bool bStaticLit,
                                                    bool bUsingFlex,
                                                    bool bUsingMorph);

// Clears out all declspecs
void ReleaseAllVertexDecl();

#endif  // MATERIALSYSTEM_SHADERAPIDX9_VERTEXDECL_H_
