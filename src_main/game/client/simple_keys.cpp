// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "cbase.h"

#include "simple_keys.h"

#include "tier0/include/memdbgon.h"

void CSimpleKeyInterp::Interp(Vector &out, float t,
                              const CSimpleKeyInterp &start,
                              const CSimpleKeyInterp &end) {
  float delta = end.GetTime() - start.GetTime();
  t = std::clamp(t - start.GetTime(), 0.0f, delta);

  float unitT = (delta > 0) ? (t / delta) : 1;

  switch (end.m_interp) {
    case KEY_SPLINE:
      unitT = SimpleSpline(unitT);
      break;
    case KEY_ACCELERATE:
      unitT *= unitT;
      break;
    case KEY_DECELERATE:
      unitT = sqrt(unitT);
      break;
    default:
    case KEY_LINEAR:
      // unitT = unitT;
      break;
  }
  out = (1 - unitT) * ((Vector)start) + unitT * ((Vector)end);
}

int CSimpleKeyList::Insert(const CSimpleKeyInterp &key) {
  for (int i = 0; i < m_list.Count(); i++) {
    if (key.GetTime() < m_list[i].GetTime()) return m_list.InsertBefore(i, key);
  }

  return m_list.AddToTail(key);
}

bool CSimpleKeyList::Interp(Vector &out, float t) {
  int startIndex = -1;

  out.Init();
  for (int i = 0; i < m_list.Count(); i++) {
    if (t < m_list[i].GetTime()) {
      // before start
      if (startIndex < 0) return false;
      CSimpleKeyInterp::Interp(out, t, m_list[startIndex], m_list[i]);
      return true;
    }
    startIndex = i;
  }

  // past end
  return false;
}
