// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef HOST_SAVERESTORE_H_
#define HOST_SAVERESTORE_H_

#include "base/include/base_types.h"
#include "tier1/KeyValues.h"
#include "tier1/interface.h"

class CSaveRestoreData;

the_interface ISaveRestore {
 public:
  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  virtual void OnFrameRendered() = 0;
  virtual bool SaveFileExists(const ch *pName) = 0;
  virtual bool LoadGame(const ch *pName) = 0;

  virtual ch *GetSaveDir() = 0;
  virtual void ClearSaveDir() = 0;
  virtual void RequestClearSaveDir() = 0;

  virtual int LoadGameState(ch const *level, bool createPlayers) = 0;
  virtual void LoadAdjacentEnts(const ch *pOldLevel,
                                const ch *pLandmarkName) = 0;

  virtual const ch *FindRecentSave(ch * pNameBuf, int nameBufLen) = 0;
  virtual void ForgetRecentSave() = 0;

  virtual int SaveGameSlot(
      const ch *pSaveName, const ch *pSaveComment, bool onlyThisLevel = false,
      bool bSetMostRecent = true, const ch *pszDestMap = nullptr,
      const ch *pszLandmark = nullptr) = 0;
  virtual bool SaveGameState(bool bTransition, CSaveRestoreData ** = nullptr,
                             bool bOpenContainer = true,
                             bool bIsAutosaveOrDangerous = false) = 0;
  virtual int IsValidSave() = 0;
  virtual void Finish(CSaveRestoreData * save) = 0;

  virtual void RestoreClientState(ch const *fileName, bool adjacent) = 0;
  virtual void RestoreAdjacenClientState(ch const *map) = 0;
  virtual int GetMostRecentElapsedMinutes() = 0;
  virtual int GetMostRecentElapsedSeconds() = 0;
  virtual int GetMostRecentElapsedTimeSet() = 0;
  virtual void SetMostRecentElapsedMinutes(const int min) = 0;
  virtual void SetMostRecentElapsedSeconds(const int sec) = 0;

  virtual void UpdateSaveGameScreenshots() = 0;

  virtual void OnFinishedClientRestore() = 0;

  virtual void AutoSaveDangerousIsSafe() = 0;

  virtual ch const *GetMostRecentlyLoadedFileName() = 0;
  virtual ch const *GetSaveFileName() = 0;

  virtual void FinishAsyncSave() = 0;
  virtual void SetMostRecentSaveGame(const ch *lpszFilename) = 0;

  virtual bool IsSaveInProgress() = 0;
};

void *SaveAllocMemory(size_t num, size_t size, bool bClear = false);
void SaveFreeMemory(void *pSaveMem);

extern ISaveRestore *saverestore;

#endif  // HOST_SAVERESTORE_H_
