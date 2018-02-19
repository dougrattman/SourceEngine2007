// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DETAIL_H
#define DETAIL_H

struct face_t;
struct tree_t;

face_t *MergeDetailTree(tree_t *worldtree, int brush_start, int brush_end);

#endif  // DETAIL_H
