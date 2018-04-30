// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "render_pch.h"

#include "draw.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "server.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier2/renderutils.h"

#include "tier0/include/memdbgon.h"

CUtlVector<Vector> g_Points;

// Draw the currently loaded line file.
void Linefile_Draw() {
  Vector *points = g_Points.Base();
  int pointCount = g_Points.Size();

  for (int i = 0; i < pointCount - 1; i++) {
    RenderLine(points[i], points[i + 1], Color(255, 255, 0, 255), true);
  }
}

// Parse the map.lin file from disk this file contains a list of line
// segments illustrating a leak in the map.
void Linefile_Read_f() {
  g_Points.Purge();

  ch name[MAX_OSPATH];
  sprintf_s(name, "maps/%s.lin", sv.GetMapName());

  CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);
  if (!g_pFileSystem->ReadFile(name, NULL, buf)) {
    ConMsg("couldn't open %s\n", name);
    return;
  }

  ConMsg("Reading %s...\n", name);
  Vector org;
  int c = 0;

  for (;;) {
    int r = buf.Scanf("%f %f %f\n", &org[0], &org[1], &org[2]);
    if (r != 3) break;
    c++;

    g_Points.AddToTail(org);
  }

  ConMsg("%i lines read\n", c);
}
