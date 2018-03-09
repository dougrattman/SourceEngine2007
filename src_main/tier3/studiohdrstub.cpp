// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "studio.h"

#include "bone_setup.h"
#include "datacache/imdlcache.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "istudiorender.h"
#include "base/include/compiler_specific.h"
#include "tier3/tier3.h"

 
#include "tier0/include/memdbgon.h"


// TODO(d.rattman): This trashy glue code is really not acceptable. Figure out a way of
// making it unnecessary.

const studiohdr_t *studiohdr_t::FindModel(void **cache,
                                          char const *pModelName) const {
  MDLHandle_t handle = g_pMDLCache->FindMDL(pModelName);
  *cache = (void *)handle;
  return g_pMDLCache->GetStudioHdr(handle);
}

virtualmodel_t *studiohdr_t::GetVirtualModel() const {
  // clang-format off
  return g_pMDLCache->GetVirtualModel(
    MSVC_SCOPED_DISABLE_WARNING(4302, reinterpret_cast<MDLHandle_t>(virtualModel)));
  // clang-format on
}

uint8_t *studiohdr_t::GetAnimBlock(int i) const {
  // clang-format off
  return g_pMDLCache->GetAnimBlock(
    MSVC_SCOPED_DISABLE_WARNING(4302, reinterpret_cast<MDLHandle_t>(virtualModel)),
      i);
  // clang-format on
}

int studiohdr_t::GetAutoplayList(unsigned short **pOut) const {
  // clang-format off
  return g_pMDLCache->GetAutoplayList(
    MSVC_SCOPED_DISABLE_WARNING(4302, reinterpret_cast<MDLHandle_t>(virtualModel)),
    pOut);
  // clang-format on
}

const studiohdr_t *virtualgroup_t::GetStudioHdr() const {
  // clang-format off
  return g_pMDLCache->GetStudioHdr(
    MSVC_SCOPED_DISABLE_WARNING(4302, reinterpret_cast<MDLHandle_t>(cache)));
  // clang-format on
}
