// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bsplib.h"

// input:
// from bsplib.h:
//		numleafs
//		dleafs

void EmitDistanceToWaterInfo() {
  int leafID;
  for (leafID = 0; leafID < numleafs; leafID++) {
    dleaf_t *pLeaf = &dleafs[leafID];
    if (pLeaf->leafWaterDataID == -1) {
      // TODO(d.rattman): set the distance to water to infinity here just in case.
      continue;
    }

    // Get the vis set for this leaf.
  }
}
