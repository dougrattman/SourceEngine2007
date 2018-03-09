// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SURFACEHANDLE_H
#define SURFACEHANDLE_H

struct msurface2_t;
typedef msurface2_t *SurfaceHandle_t;
typedef msurface2_t *SOURCE_RESTRICT SurfaceHandleRestrict_t;
const SurfaceHandle_t SURFACE_HANDLE_INVALID = NULL;
#define IS_SURF_VALID(surfID) (surfID != SURFACE_HANDLE_INVALID)

#endif  // SURFACEHANDLE_H
