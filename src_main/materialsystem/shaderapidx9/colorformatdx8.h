// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_COLORFORMATDX8_H_
#define MATERIALSYSTEM_SHADERAPIDX9_COLORFORMATDX8_H_

#include "base/include/base_types.h"
#include "base/include/windows/windows_light.h"

#include <d3d9types.h>
#include "PixelWriter.h"

// FOURCC formats for ATI shadow depth textures.
#define ATIFMT_D16 ((D3DFORMAT)(MAKEFOURCC('D', 'F', '1', '6')))
#define ATIFMT_D24S8 ((D3DFORMAT)(MAKEFOURCC('D', 'F', '2', '4')))

// FOURCC formats for ATI2N and ATI1N compressed textures (360 and DX10 parts
// also do these).
#define ATIFMT_ATI2N ((D3DFORMAT)MAKEFOURCC('A', 'T', 'I', '2'))
#define ATIFMT_ATI1N ((D3DFORMAT)MAKEFOURCC('A', 'T', 'I', '1'))

// FOURCC formats for nVidia shadow depth textures.
#define NVFMT_RAWZ ((D3DFORMAT)(MAKEFOURCC('R', 'A', 'W', 'Z')))
#define NVFMT_INTZ ((D3DFORMAT)(MAKEFOURCC('I', 'N', 'T', 'Z')))

// FOURCC format for nVidia 0 texture format.
#define NVFMT_NULL ((D3DFORMAT)(MAKEFOURCC('N', 'U', 'L', 'L')))

// Finds the nearest supported frame buffer format.
ImageFormat FindNearestSupportedBackBufferFormat(u32 adapter_idx,
                                                 D3DDEVTYPE d3d_device_type,
                                                 ImageFormat display_format,
                                                 ImageFormat back_buffer_format,
                                                 bool is_windowed);

// Initializes the color format informat; call it every time display mode
// changes.
void InitializeColorInformation(u32 adapter_idx, D3DDEVTYPE d3d_device_type,
                                ImageFormat display_format);

// Returns true if compressed textures are supported.
bool D3DSupportsCompressedTextures();

// Returns closest supported format.
ImageFormat FindNearestSupportedFormat(ImageFormat format,
                                       bool is_vertex_texture,
                                       bool is_render_target,
                                       bool is_fitlerable_required);

// Finds the nearest supported depth buffer format.
D3DFORMAT FindNearestSupportedDepthFormat(u32 adapter_idx,
                                          ImageFormat display_format,
                                          ImageFormat render_target_format,
                                          D3DFORMAT depth_format);

const ch *D3DFormatName(D3DFORMAT d3d_format);

#endif  // MATERIALSYSTEM_SHADERAPIDX9_COLORFORMATDX8_H_
