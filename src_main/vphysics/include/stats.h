// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VPHYSICS_INCLUDE_STATS_H_
#define SOURCE_VPHYSICS_INCLUDE_STATS_H_

#ifdef _WIN32
#pragma once
#endif

// Internal counters to measure cost of physics simulation.
struct physics_stats_t {
  float maxRescueSpeed;
  float maxSpeedGain;

  int impactSysNum;
  int impactCounter;
  int impactSumSys;
  int impactHardRescueCount;
  int impactRescueAfterCount;
  int impactDelayedCount;
  int impactCollisionChecks;
  int impactStaticCount;

  double totalEnergyDestroyed;
  int collisionPairsTotal;
  int collisionPairsCreated;
  int collisionPairsDestroyed;

  int potentialCollisionsObjectVsObject;
  int potentialCollisionsObjectVsWorld;

  int frictionEventsProcessed;
};

#endif  // SOURCE_VPHYSICS_INCLUDE_STATS_H_
