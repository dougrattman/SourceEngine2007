// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VPHYSICS_INCLUDE_PERFORMANCE_H_
#define SOURCE_VPHYSICS_INCLUDE_PERFORMANCE_H_

#ifdef _WIN32
#pragma once
#endif

constexpr inline float DEFAULT_MIN_FRICTION_MASS{10.0f};
constexpr inline float DEFAULT_MAX_FRICTION_MASS{2500.0f};

struct physics_performanceparams_t {
  // object will be frozen after this many collisions (visual hitching vs. CPU
  // cost)
  int maxCollisionsPerObjectPerTimestep;
  // objects may penetrate after this many collision checks (can be extended in
  // AdditionalCollisionChecksThisTick)
  int maxCollisionChecksPerTimestep;
  float maxVelocity;  // limit world space linear velocity to this (in / s)
  // limit world space angular velocity to this (degrees / s)
  float maxAngularVelocity;
  // predict collisions this far (seconds) into the future
  float lookAheadTimeObjectsVsWorld;
  // predict collisions this far (seconds) into the future
  float lookAheadTimeObjectsVsObject;
  // min mass for friction solves (constrains dynamic range of mass to improve
  // stability)
  float minFrictionMass;
  float maxFrictionMass;  // mas mass for friction solves

  void Defaults() {
    maxCollisionsPerObjectPerTimestep = 6;
    maxCollisionChecksPerTimestep = 250;
    maxVelocity = 2000.0f;
    maxAngularVelocity = 360.0f * 10.0f;
    lookAheadTimeObjectsVsWorld = 1.0f;
    lookAheadTimeObjectsVsObject = 0.5f;
    minFrictionMass = DEFAULT_MIN_FRICTION_MASS;
    maxFrictionMass = DEFAULT_MAX_FRICTION_MASS;
  }
};

#endif  // SOURCE_VPHYSICS_INCLUDE_PERFORMANCE_H_
