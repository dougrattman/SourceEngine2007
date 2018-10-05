// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VPHYSICS_INCLUDE_COLLISION_SET_H_
#define SOURCE_VPHYSICS_INCLUDE_COLLISION_SET_H_

// A set of collision rules.
// NOTE: Defaults to all indices disabled
class IPhysicsCollisionSet {
 public:
  ~IPhysicsCollisionSet() {}

  virtual void EnableCollisions(int index0, int index1) = 0;
  virtual void DisableCollisions(int index0, int index1) = 0;

  virtual bool ShouldCollide(int index0, int index1) = 0;
};

#endif  // !SOURCE_VPHYSICS_INCLUDE_COLLISION_SET_H_