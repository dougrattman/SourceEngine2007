// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "raytrace.h"

#include "bspfile.h"
#include "bsplib.h"

static Vector VertCoord(dface_t const &f, int vnum) {
  int eIndex = dsurfedges[f.firstedge + vnum];
  int point = eIndex < 0 ? dedges[-eIndex].v[1] : dedges[eIndex].v[0];

  dvertex_t *v = dvertexes + point;
  return Vector(v->point[0], v->point[1], v->point[2]);
}

Vector colors[] = {Vector(0.5, 0.5, 1), Vector(0.5, 1, 0.5), Vector(0.5, 1, 1),
                   Vector(1, 0.5, 0.5), Vector(1, 0.5, 1),   Vector(1, 1, 1)};

void RayTracingEnvironment::AddBSPFace(int id, dface_t const &face) {
  // displacements must be dealt with elsewhere
  if (face.dispinfo != -1) return;

  texinfo_t *tx = (face.texinfo >= 0) ? &(texinfo[face.texinfo]) : 0;
  if (tx) printf("id %d flags=%x\n", id, tx->flags);

  printf("side: ");
  for (int v = 0; v < face.numedges; v++) {
    printf("(%f %f %f) ", XYZ(VertCoord(face, v)));
  }
  printf("\n");

  int ntris = face.numedges - 2;
  for (int tri = 0; tri < ntris; tri++) {
    AddTriangle(id, VertCoord(face, 0),
                VertCoord(face, (tri + 1) % face.numedges),
                VertCoord(face, (tri + 2) % face.numedges), Vector{1, 1, 1});
  }
}

void RayTracingEnvironment::InitializeFromLoadedBSP() {
  for (int c = 0; c < numfaces; c++) {
    AddBSPFace(c, dorigfaces[c]);
  }
}
