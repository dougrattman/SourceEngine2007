// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DEVSHOTGENERATOR_H
#define DEVSHOTGENERATOR_H

#include "MapReslistGenerator.h"
#include "filesystem.h"
#include "tier1/UtlVector.h"
#include "tier1/utlsymbol.h"

// initialization
void DevShotGenerator_Init();
void DevShotGenerator_Shutdown();
void DevShotGenerator_BuildMapList();

//-----------------------------------------------------------------------------
// Purpose: Takes development screenshots at camera positions over a set of
// levels
//-----------------------------------------------------------------------------
class CDevShotGenerator {
 public:
  CDevShotGenerator();

  // initializes the object to enable devshot generation
  void EnableDevShotGeneration(bool usemaplistfile);

  // starts the dev shot generation (starts cycling maps)
  void StartDevShotGeneration();

  void BuildMapList();
  void NextMap();

  void Shutdown();

  // returns true if reslist generation is enabled
  bool IsEnabled() { return m_bDevShotsEnabled; }

 private:
  bool m_bDevShotsEnabled;
  int m_iCurrentMap;
  bool m_bUsingMapList;

  // list of all maps to scan
  CUtlVector<maplist_map_t> m_Maps;
};

// singleton accessor
CDevShotGenerator &DevShotGenerator();

#endif  // DEVSHOTGENERATOR_H
