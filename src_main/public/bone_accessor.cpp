// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bone_accessor.h"
#include "cbase.h"

#if defined(CLIENT_DLL) && defined(_DEBUG)

void CBoneAccessor::SanityCheckBone(int iBone, bool bReadable) const {
  if (m_pAnimating) {
    CStudioHdr *pHdr = m_pAnimating->GetModelPtr();
    if (pHdr) {
      mstudiobone_t *pBone = pHdr->pBone(iBone);
      if (bReadable) {
        AssertOnce(pBone->flags & m_ReadableBones);
      } else {
        AssertOnce(pBone->flags & m_WritableBones);
      }
    }
  }
}

#endif
