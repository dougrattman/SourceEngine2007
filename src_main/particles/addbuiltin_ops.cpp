// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "particles/particles.h"

void AddBuiltInParticleOperators();
void AddBuiltInParticleRenderers();
void AddBuiltInParticleInitializers();
void AddBuiltInParticleEmitters();
void AddBuiltInParticleForceGenerators();
void AddBuiltInParticleConstraints();

void CParticleSystemMgr::AddBuiltinSimulationOperators() {
  static bool s_DidAddSim = false;
  if (!s_DidAddSim) {
    s_DidAddSim = true;
    AddBuiltInParticleOperators();
    AddBuiltInParticleInitializers();
    AddBuiltInParticleEmitters();
    AddBuiltInParticleForceGenerators();
    AddBuiltInParticleConstraints();
  }
}

void CParticleSystemMgr::AddBuiltinRenderingOperators() {
  static bool s_DidAddRenderers = false;
  if (!s_DidAddRenderers) {
    s_DidAddRenderers = true;
    AddBuiltInParticleRenderers();
  }
}
