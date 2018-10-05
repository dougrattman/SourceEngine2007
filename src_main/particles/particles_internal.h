// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: sheet code for particles and other sprite functions

#ifndef SOURCE_PARTICLES_PARTICLES_INTERNAL_H_
#define SOURCE_PARTICLES_PARTICLES_INTERNAL_H_

#include "base/include/base_types.h"
#include "tier1/utlstringmap.h"
#include "tier1/utlbuffer.h"
#include "tier2/fileutils.h"

#define MAX_WORLD_PLANAR_CONSTRAINTS 26

#define COLLISION_MODE_PER_PARTICLE_TRACE 0
#define COLLISION_MODE_PER_FRAME_PLANESET 1
#define COLLISION_MODE_INITIAL_TRACE_DOWN 2

struct CWorldCollideContextData {
  i32 m_nActivePlanes;
  f32 m_flLastUpdateTime;
  Vector m_vecLastUpdateOrigin;

  FourVectors m_PointOnPlane[MAX_WORLD_PLANAR_CONSTRAINTS];
  FourVectors m_PlaneNormal[MAX_WORLD_PLANAR_CONSTRAINTS];

  void *operator new(size_t nSize);
  void *operator new(size_t nSize, int nBlockUse, const char *pFileName,
                     int nLine);
  void operator delete(void *pData);
  void operator delete(void *p, int nBlockUse, const char *pFileName,
                       int nLine);

  void CalculatePlanes(CParticleCollection *pParticles, i32 nCollisionMode,
                       i32 nCollisionGroupNumber,
                       Vector const *pCpOffset = nullptr,
                       f32 flMovementTolerance = 0.0f);
};

#endif  // SOURCE_PARTICLES_PARTICLES_INTERNAL_H_
