// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include "studio.h"

// NOTE: You can change this without affecting the vtx file format.
#define MAX_NUM_BONES_PER_TRI (MAX_NUM_BONES_PER_VERT * 3)
#define MAX_NUM_BONES_PER_STRIP 512

#define OPTIMIZED_MODEL_FILE_VERSION 7

extern bool g_bDumpGLViewFiles;

struct s_bodypart_t;

namespace OptimizedModel {

#pragma pack(1)

struct BoneStateChangeHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  int hardwareID;
  int newBoneID;
};

struct Vertex_t {
  DECLARE_BYTESWAP_DATADESC();
  // these index into the mesh's vert[origMeshVertID]'s bones
  u8 boneWeightIndex[MAX_NUM_BONES_PER_VERT];
  u8 numBones;

  u16 origMeshVertID;

  // for sw skinned verts, these are indices into the global list of bones
  // for hw skinned verts, these are hardware bone indices
  char boneID[MAX_NUM_BONES_PER_VERT];
};

enum StripHeaderFlags_t { STRIP_IS_TRILIST = 0x01, STRIP_IS_TRISTRIP = 0x02 };

// a strip is a piece of a stripgroup that is divided by bones
// (and potentially tristrips if we remove some degenerates.)
struct StripHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  // indexOffset offsets into the mesh's index array.
  int numIndices;
  int indexOffset;

  // vertexOffset offsets into the mesh's vert array.
  int numVerts;
  int vertOffset;

  // use this to enable/disable skinning.
  // May decide (in optimize.cpp) to put all with 1 bone in a different strip
  // than those that need skinning.
  short numBones;

  u8 flags;

  int numBoneStateChanges;
  int boneStateChangeOffset;
  inline BoneStateChangeHeader_t *pBoneStateChange(int i) const {
    return (BoneStateChangeHeader_t *)(((u8 *)this) +
                                       boneStateChangeOffset) +
           i;
  };
};

enum StripGroupFlags_t {
  STRIPGROUP_IS_FLEXED = 0x01,
  STRIPGROUP_IS_HWSKINNED = 0x02,
  STRIPGROUP_IS_DELTA_FLEXED = 0x04,
  STRIPGROUP_SUPPRESS_HW_MORPH =
      0x08,  // NOTE: This is a temporary flag used at run time.
};

// a locking group
// a single vertex buffer
// a single index buffer
struct StripGroupHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  // These are the arrays of all verts and indices for this mesh.  strips index
  // into this.
  int numVerts;
  int vertOffset;
  inline Vertex_t *pVertex(int i) const {
    return (Vertex_t *)(((u8 *)this) + vertOffset) + i;
  };

  int numIndices;
  int indexOffset;
  inline u16 *pIndex(int i) const {
    return (u16 *)(((u8 *)this) + indexOffset) + i;
  };

  int numStrips;
  int stripOffset;
  inline StripHeader_t *pStrip(int i) const {
    return (StripHeader_t *)(((u8 *)this) + stripOffset) + i;
  };

  u8 flags;
};

enum MeshFlags_t {
  // these are both material properties, and a mesh has a single material.
  MESH_IS_TEETH = 0x01,
  MESH_IS_EYES = 0x02
};

// a collection of locking groups:
// up to 4:
// non-flexed, hardware skinned
// flexed, hardware skinned
// non-flexed, software skinned
// flexed, software skinned
//
// A mesh has a material associated with it.
struct MeshHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  int numStripGroups;
  int stripGroupHeaderOffset;
  inline StripGroupHeader_t *pStripGroup(int i) const {
    StripGroupHeader_t *pDebug =
        (StripGroupHeader_t *)(((u8 *)this) + stripGroupHeaderOffset) + i;
    return pDebug;
  };
  u8 flags;
};

struct ModelLODHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  int numMeshes;
  int meshOffset;
  f32 switchPoint;
  inline MeshHeader_t *pMesh(int i) const {
    MeshHeader_t *pDebug = (MeshHeader_t *)(((u8 *)this) + meshOffset) + i;
    return pDebug;
  };
};

// This maps one to one with models in the mdl file.
// There are a bunch of model LODs stored inside potentially due to the qc $lod
// command
struct ModelHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  int numLODs;  // garymcthack - this is also specified in FileHeader_t
  int lodOffset;
  inline ModelLODHeader_t *pLOD(int i) const {
    ModelLODHeader_t *pDebug =
        (ModelLODHeader_t *)(((u8 *)this) + lodOffset) + i;
    return pDebug;
  };
};

struct BodyPartHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  int numModels;
  int modelOffset;
  inline ModelHeader_t *pModel(int i) const {
    ModelHeader_t *pDebug =
        (ModelHeader_t *)(((u8 *)this) + modelOffset) + i;
    return pDebug;
  };
};

struct MaterialReplacementHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  short materialID;
  int replacementMaterialNameOffset;
  inline const char *pMaterialReplacementName() {
    const char *pDebug =
        (const char *)(((u8 *)this) + replacementMaterialNameOffset);
    return pDebug;
  }
};

struct MaterialReplacementListHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  int numReplacements;
  int replacementOffset;
  inline MaterialReplacementHeader_t *pMaterialReplacement(int i) const {
    MaterialReplacementHeader_t *pDebug =
        (MaterialReplacementHeader_t *)(((u8 *)this) + replacementOffset) +
        i;
    return pDebug;
  }
};

struct FileHeader_t {
  DECLARE_BYTESWAP_DATADESC();
  // file version as defined by OPTIMIZED_MODEL_FILE_VERSION
  int version;

  // hardware params that affect how the model is to be optimized.
  int vertCacheSize;
  u16 maxBonesPerStrip;
  u16 maxBonesPerTri;
  int maxBonesPerVert;

  // must match checkSum in the .mdl
  long checkSum;

  int numLODs;  // garymcthack - this is also specified in ModelHeader_t and
                // should match

  // one of these for each LOD
  int materialReplacementListOffset;
  MaterialReplacementListHeader_t *pMaterialReplacementList(int lodID) const {
    MaterialReplacementListHeader_t *pDebug =
        (MaterialReplacementListHeader_t *)(((u8 *)this) +
                                            materialReplacementListOffset) +
        lodID;
    return pDebug;
  }

  int numBodyParts;
  int bodyPartOffset;
  inline BodyPartHeader_t *pBodyPart(int i) const {
    BodyPartHeader_t *pDebug =
        (BodyPartHeader_t *)(((u8 *)this) + bodyPartOffset) + i;
    return pDebug;
  };
};

#pragma pack()

void WriteOptimizedFiles(studiohdr_t *phdr, s_bodypart_t *pSrcBodyParts);

};  // namespace OptimizedModel

#endif  // OPTIMIZE_H
