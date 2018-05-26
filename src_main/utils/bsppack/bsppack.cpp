// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "ibsppack.h"

#include "bsplib.h"
#include "cmdlib.h"

class CBSPPack : public IBSPPack {
 public:
  void LoadBSPFile(IFileSystem *file_system, char *file_name) override {
    MathLib_Init();

    SetFileSystems(file_system);

    ::LoadBSPFile(file_name);
  }

  void WriteBSPFile(char *file_name) override { ::WriteBSPFile(file_name); }

  void ClearPackFile() override { ::ClearPakFile(GetPakFile()); }

  void AddFileToPack(const char *relative_name,
                     const char *full_file_path) override {
    ::AddFileToPak(GetPakFile(), relative_name, full_file_path);
  }

  void AddBufferToPack(const char *relative_name, void *data, int length,
                       bool bTextMode) override {
    ::AddBufferToPak(GetPakFile(), relative_name, data, length, bTextMode);
  }

  void SetHDRMode(bool bHDR) override { ::SetHDRMode(bHDR); }

  bool SwapBSPFile(IFileSystem *file_system, const char *file_name,
                   const char *swapFilename, bool bSwapOnLoad,
                   VTFConvertFunc_t pVTFConvertFunc,
                   VHVFixupFunc_t pVHVFixupFunc,
                   CompressFunc_t pCompressFunc) override {
    SetFileSystems(file_system);
    return ::SwapBSPFile(file_name, swapFilename, bSwapOnLoad, pVTFConvertFunc,
                         pVHVFixupFunc, pCompressFunc);
  }

  bool GetPakFileLump(IFileSystem *file_system, const char *pBSPFilename,
                      void **pPakData, int *pPakSize) override {
    SetFileSystems(file_system);
    return ::GetPakFileLump(pBSPFilename, pPakData, pPakSize);
  }

  bool SetPakFileLump(IFileSystem *file_system, const char *pBSPFilename,
                      const char *pNewFilename, void *pPakData,
                      int pakSize) override {
    SetFileSystems(file_system);
    return ::SetPakFileLump(pBSPFilename, pNewFilename, pPakData, pakSize);
  }

  bool GetBSPDependants(IFileSystem *file_system, const char *pBSPFilename,
                        CUtlVector<CUtlString> *pList) override {
    SetFileSystems(file_system);
    return ::GetBSPDependants(pBSPFilename, pList);
  }

 private:
  static inline void SetFileSystems(IFileSystem *file_system) {
    // This is shady, but the engine is the only client here and we want the
    // same search paths it has.
    g_pFileSystem = g_pFullFileSystem = file_system;
  }
};

EXPOSE_SINGLE_INTERFACE(CBSPPack, IBSPPack, IBSPPACK_VERSION_STRING);
