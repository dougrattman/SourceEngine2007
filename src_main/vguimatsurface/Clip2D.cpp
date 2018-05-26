// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Contains 2D clipping routines

#include "Clip2D.h"
#include "tier0/include/dbg.h"
#include "tier1/UtlVector.h"
#include "vgui/isurface.h"

#include "tier0/include/memdbgon.h"

// Stretch texture to fit window ( before scissoring )
static bool g_bStretchTexture = false;

// Max # of vertices for clipping
enum {
  VGUI_VERTEX_TEMP_COUNT = 48,
};

// For simulated scissor tests...
struct ScissorRect_t {
  int left;
  int top;
  int right;
  int bottom;
};

static ScissorRect_t g_ScissorRect;
static bool g_bScissor = false;
static bool g_bFullScreenScissor = false;

// Enable/disable scissoring...
void EnableScissor(bool enable) { g_bScissor = enable; }

void SetScissorRect(int left, int top, int right, int bottom) {
  // Check for a valid rectangle...
  Assert(left <= right);
  Assert(top <= bottom);

  g_ScissorRect.left = left;
  g_ScissorRect.top = top;
  g_ScissorRect.right = right;
  g_ScissorRect.bottom = bottom;
}

void GetScissorRect(int& left, int& top, int& right, int& bottom,
                    bool& enabled) {
  left = g_ScissorRect.left;
  top = g_ScissorRect.top;
  right = g_ScissorRect.right;
  bottom = g_ScissorRect.bottom;
  enabled = g_bScissor;
}

// Used to clip the shadow decals
struct PolygonClipState_t {
  int m_CurrVert;
  int m_TempCount;
  int m_ClipCount;
  vgui::Vertex_t m_pTempVertices[VGUI_VERTEX_TEMP_COUNT];
  vgui::Vertex_t* m_ppClipVertices[2][VGUI_VERTEX_TEMP_COUNT];
};

// Clipping methods for 2D
struct CClipTop {
  static inline bool Inside(vgui::Vertex_t const& vert) {
    return vert.m_Position.y >= g_ScissorRect.top;
  }

  static inline float Clip(const Vector2D& one, const Vector2D& two) {
    return (g_ScissorRect.top - one.y) / (two.y - one.y);
  }
};

struct CClipLeft {
  static inline bool Inside(vgui::Vertex_t const& vert) {
    return vert.m_Position.x >= g_ScissorRect.left;
  }

  static inline float Clip(const Vector2D& one, const Vector2D& two) {
    return (one.x - g_ScissorRect.left) / (one.x - two.x);
  }
};

struct CClipRight {
  static inline bool Inside(vgui::Vertex_t const& vert) {
    return vert.m_Position.x < g_ScissorRect.right;
  }

  static inline float Clip(const Vector2D& one, const Vector2D& two) {
    return (g_ScissorRect.right - one.x) / (two.x - one.x);
  }
};

struct CClipBottom {
  static inline bool Inside(vgui::Vertex_t const& vert) {
    return vert.m_Position.y < g_ScissorRect.bottom;
  }

  static inline float Clip(const Vector2D& one, const Vector2D& two) {
    return (one.y - g_ScissorRect.bottom) / (one.y - two.y);
  }
};

template <typename Clipper>
static inline void Intersect(const vgui::Vertex_t& start,
                             const vgui::Vertex_t& end, vgui::Vertex_t* pOut) {
  // Clip to the scissor rectangle
  float t = Clipper::Clip(start.m_Position, end.m_Position);
  Vector2DLerp(start.m_Position, end.m_Position, t, pOut->m_Position);
  Vector2DLerp(start.m_TexCoord, end.m_TexCoord, t, pOut->m_TexCoord);
}

// Clips a line segment to a single plane
template <typename Clipper>
bool ClipLineToPlane(const vgui::Vertex_t* pInVerts,
                     vgui::Vertex_t* pOutVerts) {
  bool startInside = Clipper::Inside(pInVerts[0]);
  bool endInside = Clipper::Inside(pInVerts[1]);

  // Cull
  if (!startInside && !endInside) return false;

  if (startInside && endInside) {
    pOutVerts[0] = pInVerts[0];
    pOutVerts[1] = pInVerts[1];
  } else {
    int inIndex = startInside ? 0 : 1;
    pOutVerts[inIndex] = pInVerts[inIndex];
    Intersect<Clipper>(pInVerts[0], pInVerts[1], &pOutVerts[1 - inIndex]);
  }

  return true;
}

// Clips a line segment to the current scissor rectangle
bool ClipLine(const vgui::Vertex_t* pInVerts, vgui::Vertex_t* pOutVerts) {
  if (g_bScissor && !g_bFullScreenScissor) {
    // Sutherland-hodgman clip, not particularly efficient but that's ok for now
    vgui::Vertex_t tempVerts[2];

    if (!ClipLineToPlane<CClipTop>(pInVerts, tempVerts)) return false;
    if (!ClipLineToPlane<CClipBottom>(tempVerts, pOutVerts)) return false;
    if (!ClipLineToPlane<CClipLeft>(pOutVerts, tempVerts)) return false;
    if (!ClipLineToPlane<CClipRight>(tempVerts, pOutVerts)) return false;

    return true;
  }

  pOutVerts[0] = pInVerts[0];
  pOutVerts[1] = pInVerts[1];
  return true;
}

// Methods associated with clipping 2D polygons
struct ScreenClipState_t {
  int m_iCurrVert;
  int m_iTempCount;
  int m_iClipCount;
  CUtlVector<vgui::Vertex_t> m_pTempVertices;
  CUtlVector<vgui::Vertex_t*> m_ppClipVertices[2];
};

template <typename Clipper>
static void ScreenClip(ScreenClipState_t& clip) {
  if (clip.m_iClipCount < 3) return;

  // Ye Olde Sutherland-Hodgman clipping algorithm
  int numOutVerts = 0;
  vgui::Vertex_t** pSrcVert = clip.m_ppClipVertices[clip.m_iCurrVert].Base();
  vgui::Vertex_t** pDestVert = clip.m_ppClipVertices[!clip.m_iCurrVert].Base();

  int numVerts = clip.m_iClipCount;
  vgui::Vertex_t* pStart = pSrcVert[numVerts - 1];
  bool startInside = Clipper::Inside(*pStart);
  for (int i = 0; i < numVerts; ++i) {
    vgui::Vertex_t* pEnd = pSrcVert[i];
    bool endInside = Clipper::Inside(*pEnd);
    if (endInside) {
      if (!startInside) {
        // Started outside, ended inside, need to clip the edge
        Assert(clip.m_iTempCount <= clip.m_pTempVertices.Count());

        // Allocate a new clipped vertex
        pDestVert[numOutVerts] = &clip.m_pTempVertices[clip.m_iTempCount++];

        // Clip the edge to the clip plane
        Intersect<Clipper>(*pStart, *pEnd, pDestVert[numOutVerts]);
        ++numOutVerts;
      }
      pDestVert[numOutVerts++] = pEnd;
    } else {
      if (startInside) {
        // Started inside, ended outside, need to clip the edge
        Assert(clip.m_iTempCount <= clip.m_pTempVertices.Count());

        // Allocate a new clipped vertex
        pDestVert[numOutVerts] = &clip.m_pTempVertices[clip.m_iTempCount++];

        // Clip the edge to the clip plane
        Intersect<Clipper>(*pStart, *pEnd, pDestVert[numOutVerts]);
        ++numOutVerts;
      }
    }
    pStart = pEnd;
    startInside = endInside;
  }

  // Switch source lists
  clip.m_iCurrVert = 1 - clip.m_iCurrVert;
  clip.m_iClipCount = numOutVerts;
}

// Clips a polygon to the screen area
int ClipPolygon(int iCount, vgui::Vertex_t* pVerts, int iTranslateX,
                int iTranslateY, vgui::Vertex_t*** pppOutVertex) {
  static ScreenClipState_t clip;

  // Allocate enough room in the clip state...
  // Having no reallocations during clipping
  clip.m_pTempVertices.EnsureCount(iCount * 4);
  clip.m_ppClipVertices[0].EnsureCount(iCount * 4);
  clip.m_ppClipVertices[1].EnsureCount(iCount * 4);

  // Copy the initial verts in...
  for (int i = 0; i < iCount; ++i) {
    // NOTE: This only works because we EnsuredCount above
    clip.m_pTempVertices[i] = pVerts[i];
    clip.m_pTempVertices[i].m_Position.x += iTranslateX;
    clip.m_pTempVertices[i].m_Position.y += iTranslateY;
    clip.m_ppClipVertices[0][i] = &clip.m_pTempVertices[i];
  }

  if (!g_bScissor || g_bFullScreenScissor) {
    Assert(pppOutVertex);
    *pppOutVertex = clip.m_ppClipVertices[0].Base();
    return iCount;
  }

  clip.m_iClipCount = iCount;
  clip.m_iTempCount = iCount;
  clip.m_iCurrVert = 0;

  // Sutherland-hodgman clip
  ScreenClip<CClipTop>(clip);
  ScreenClip<CClipBottom>(clip);
  ScreenClip<CClipLeft>(clip);
  ScreenClip<CClipRight>(clip);

  if (clip.m_iClipCount < 3) return 0;

  // Return a pointer to the array of clipped vertices...
  Assert(pppOutVertex);
  *pppOutVertex = clip.m_ppClipVertices[clip.m_iCurrVert].Base();
  return clip.m_iClipCount;
}

// Purpose: Used for clipping, produces an interpolated texture coordinate
inline float InterpTCoord(float val, float mins, float maxs, float tMin,
                          float tMax) {
  const float flPercent = mins != maxs ? (val - mins) / (maxs - mins) : 0.5f;
  return tMin + (tMax - tMin) * flPercent;
}

// Purpose: Does a scissor clip of the input rectangle.  Returns false if it is
// completely clipped off.
bool ClipRect(const vgui::Vertex_t& inUL, const vgui::Vertex_t& inLR,
              vgui::Vertex_t* pOutUL, vgui::Vertex_t* pOutLR) {
  // Check for a valid rectangle...
  Assert(inUL.m_Position.x <= inLR.m_Position.x);
  Assert(inUL.m_Position.y <= inLR.m_Position.y);

  if (g_bScissor) {
    // Pick whichever left side is larger
    pOutUL->m_Position.x = (g_ScissorRect.left > inUL.m_Position.x)
                               ? g_ScissorRect.left
                               : inUL.m_Position.x;

    // Pick whichever right side is smaller
    pOutLR->m_Position.x = (g_ScissorRect.right <= inLR.m_Position.x)
                               ? g_ScissorRect.right
                               : inLR.m_Position.x;

    // Pick whichever top side is larger
    pOutUL->m_Position.y = (g_ScissorRect.top > inUL.m_Position.y)
                               ? g_ScissorRect.top
                               : inUL.m_Position.y;

    // Pick whichever bottom side is smaller
    pOutLR->m_Position.y = (g_ScissorRect.bottom <= inLR.m_Position.y)
                               ? g_ScissorRect.bottom
                               : inLR.m_Position.y;

    // Check for non-intersecting
    if ((pOutUL->m_Position.x > pOutLR->m_Position.x) ||
        (pOutUL->m_Position.y > pOutLR->m_Position.y)) {
      return false;
    }

    if (!g_bStretchTexture) {
      pOutUL->m_TexCoord.x =
          InterpTCoord(pOutUL->m_Position.x, inUL.m_Position.x,
                       inLR.m_Position.x, inUL.m_TexCoord.x, inLR.m_TexCoord.x);
      pOutLR->m_TexCoord.x =
          InterpTCoord(pOutLR->m_Position.x, inUL.m_Position.x,
                       inLR.m_Position.x, inUL.m_TexCoord.x, inLR.m_TexCoord.x);

      pOutUL->m_TexCoord.y =
          InterpTCoord(pOutUL->m_Position.y, inUL.m_Position.y,
                       inLR.m_Position.y, inUL.m_TexCoord.y, inLR.m_TexCoord.y);
      pOutLR->m_TexCoord.y =
          InterpTCoord(pOutLR->m_Position.y, inUL.m_Position.y,
                       inLR.m_Position.y, inUL.m_TexCoord.y, inLR.m_TexCoord.y);
    } else {
      // TODO(d.rattman): this isn't right
      pOutUL->m_TexCoord = inUL.m_TexCoord;
      pOutLR->m_TexCoord = inLR.m_TexCoord;
    }
  } else {
    *pOutUL = inUL;
    *pOutLR = inLR;
  }

  return true;
}
