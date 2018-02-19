// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VISMAT_H
#define VISMAT_H

void BuildVisLeafs(int threadnum);

// MPI uses these.
struct transfer_t;
transfer_t *BuildVisLeafs_Start();

// If PatchCB is non-null, it is called after each row is generated (used by
// MPI).
void BuildVisLeafs_Cluster(int threadnum, transfer_t *transfers, int iCluster,
                           void (*PatchCB)(int iThread, int patchnum,
                                           CPatch *patch));

void BuildVisLeafs_End(transfer_t *transfers);

#endif  // VISMAT_H
