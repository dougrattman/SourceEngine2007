// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER2_BEAMSEGDRAW_H_
#define SOURCE_TIER2_BEAMSEGDRAW_H_

#ifdef _WIN32
#pragma once
#endif

#define NOISE_DIVISIONS 128

#include "base/include/base_types.h"
#include "materialsystem/imesh.h"
#include "mathlib/vector.h"

struct BeamTrail_t;
the_interface IMaterial;

// CBeamSegDraw is a simple interface to beam rendering.
struct BeamSeg_t {
  Vector m_vPos;
  Vector m_vColor;
  f32 m_flTexCoord;  // Y texture coordinate
  f32 m_flWidth;
  f32 m_flAlpha;
};

class CBeamSegDraw {
 public:
  CBeamSegDraw()
      : m_pMeshBuilder{nullptr},
        m_nMeshVertCount{0},
        m_nTotalSegs{0},
        m_nSegsDrawn{0},
        m_pRenderContext{nullptr} {}

  // Pass 0 for pMaterial if you have already set the material you want.
  void Start(IMatRenderContext *render_context, int segments_count,
             IMaterial *material = nullptr,
             CMeshBuilder *mesh_builder = nullptr, int mesh_vertex_count = 0);
  virtual void NextSeg(BeamSeg_t *pSeg);
  void End();

 protected:
  void SpecifySeg(const Vector &camera_pos, const Vector &next_pos);
  void ComputeNormal(const Vector &camera_pos, const Vector &start_pos,
                     const Vector &next_pos, Vector *normal);

  CMeshBuilder *m_pMeshBuilder;
  int m_nMeshVertCount;

  CMeshBuilder m_Mesh;
  BeamSeg_t m_Seg;

  int m_nTotalSegs;
  int m_nSegsDrawn;

  Vector m_vNormalLast;
  IMatRenderContext *m_pRenderContext;
};

class CBeamSegDrawArbitrary : public CBeamSegDraw {
 public:
  void SetNormal(const Vector &normal);
  void NextSeg(BeamSeg_t *beam_seg);

 protected:
  void SpecifySeg(const Vector &next_pos);

  BeamSeg_t m_PrevSeg;
};

// Assumes the material has already been bound.
void DrawSprite(const Vector &origin, f32 width, f32 height, color32 color);

#endif  // SOURCE_TIER2_BEAMSEGDRAW_H_