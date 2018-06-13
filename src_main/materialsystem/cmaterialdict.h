// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATERIALSYSTEM_CMATERIALDICT_H_
#define SOURCE_MATERIALSYSTEM_CMATERIALDICT_H_

#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"

#ifndef MATSYS_INTERNAL
#error \
    "This file is private to the implementation of IMaterialSystem/IMaterialSystemInternal"
#endif

class IMaterial;
class IMaterialInternal;
typedef u16 MaterialHandle_t;

// Dictionary of all known materials
class CMaterialDict {
 public:
  CMaterialDict()
      : m_MaterialDict(0, 256, MaterialLessFunc),
        m_MissingList(0, 32, MissingMaterialLessFunc) {}

  void Shutdown();

  int GetNumMaterials(void) const { return m_MaterialDict.Count(); }

  // Handle to material
  IMaterial* GetMaterial(MaterialHandle_t h) const {
    return m_MaterialDict[h].m_pMaterial;
  }

  IMaterialInternal* GetMaterialInternal(MaterialHandle_t idx) const {
    Assert((m_MaterialDict[idx].m_pMaterial == nullptr) ||
           m_MaterialDict[idx].m_pMaterial->IsRealTimeVersion());
    return m_MaterialDict[idx].m_pMaterial;
  }

  // Material iteration methods
  MaterialHandle_t FirstMaterial() const {
    return m_MaterialDict.FirstInorder();
  }
  MaterialHandle_t NextMaterial(MaterialHandle_t h) const {
    return m_MaterialDict.NextInorder(h);
  }

  // Invalid index handle....
  MaterialHandle_t InvalidMaterial() const {
    return m_MaterialDict.InvalidIndex();
  }

  IMaterialInternal* FindMaterial(const char* pszName,
                                  bool bManuallyCreated) const {
    MaterialLookup_t lookup;
    lookup.m_Name = pszName;
    lookup.m_bManuallyCreated = bManuallyCreated;  // This causes the search to
                                                   // find only file-created
                                                   // materials

    MaterialHandle_t h = m_MaterialDict.Find(lookup);

    if (h != m_MaterialDict.InvalidIndex()) {
      return m_MaterialDict[h].m_pMaterial;
    }

    return nullptr;
  }

  void AddMaterialToMaterialList(IMaterialInternal* pMaterial);
  void RemoveMaterialFromMaterialList(IMaterialInternal* pMaterial);

  void RemoveMaterial(IMaterialInternal* pMaterial);
  void RemoveMaterialSubRect(IMaterialInternal* pMaterial);

  IMaterialInternal* AddMaterial(const char* pName,
                                 const char* pTextureGroupName);
  // pKeyValues and pPatchKeyValues should come from LoadVMTFile()
  IMaterialInternal* AddMaterialSubRect(const char* pName,
                                        const char* pTextureGroupName,
                                        KeyValues* pKeyValues,
                                        KeyValues* pPatchKeyValues);

  bool NoteMissing(const char* pszName) {
    MissingMaterial_t missing;
    missing.m_Name = pszName;
    if (m_MissingList.Find(missing) != m_MissingList.InvalidIndex()) {
      return false;
    }

    m_MissingList.Insert(missing);
    return true;
  }

 protected: /*private:*/
  void RemoveAllMaterials();
  void RemoveAllMaterialsFromMaterialList();
  void RemoveMaterialFromMaterialList(MaterialHandle_t h);

  // Stores a dictionary of materials, searched by name
  struct MaterialLookup_t {
    IMaterialInternal* m_pMaterial;
    CUtlSymbol m_Name;
    bool m_bManuallyCreated;
  };

  // Stores a dictionary of missing materials to cut down on redundant warning
  // messages
  // TODO:  1) Could add a counter
  //        2) Could dump to file/console at exit for exact list of missing
  //        materials
  struct MissingMaterial_t {
    CUtlSymbol m_Name;
  };

  static bool MaterialLessFunc(const MaterialLookup_t& src1,
                               const MaterialLookup_t& src2);

  static bool MissingMaterialLessFunc(const MissingMaterial_t& src1,
                                      const MissingMaterial_t& src2);

  CUtlRBTree<MaterialLookup_t, MaterialHandle_t> m_MaterialDict;
  CUtlRBTree<MissingMaterial_t, int> m_MissingList;
};

#endif  // SOURCE_MATERIALSYSTEM_CMATERIALDICT_H_
