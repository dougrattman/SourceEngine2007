// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASESAVEGAMEDIALOG_H
#define BASESAVEGAMEDIALOG_H

#include "base/include/base_types.h"
#include "tier1/UtlVector.h"
#include "tier1/keyvalues.h"
#include "vgui/MouseCode.h"
#include "vgui_controls/Frame.h"

#define SAVEGAME_MAPNAME_LEN 32
#define SAVEGAME_COMMENT_LEN 80
#define SAVEGAME_ELAPSED_LEN 32

struct SaveGameDescription_t {
  ch szShortName[64];
  ch szFileName[128];
  ch szMapName[SAVEGAME_MAPNAME_LEN];
  ch szComment[SAVEGAME_COMMENT_LEN];
  ch szType[64];
  ch szElapsedTime[SAVEGAME_ELAPSED_LEN];
  ch szFileTime[32];
  u32 iTimestamp;
  u32 iSize;
};

template <usize name_size, usize comment_size>
bool SaveReadNameAndComment(FileHandle_t f, ch (&name)[name_size],
                            ch (&comment)[comment_size]);

// Base class for save & load game dialogs.
class CBaseSaveGameDialog : public vgui::Frame {
  DECLARE_CLASS_SIMPLE(CBaseSaveGameDialog, vgui::Frame);

 public:
  CBaseSaveGameDialog(vgui::Panel *parent, const ch *name);
  static i32 __cdecl SaveGameSortFunc(const void *lhs, const void *rhs);

 protected:
  CUtlVector<SaveGameDescription_t> m_SaveGames;
  vgui::PanelListPanel *m_pGameList;

  virtual void OnScanningSaveGames() {}

  void DeleteSaveGame(const ch *fileName);
  void ScanSavedGames();
  void CreateSavedGamesList();
  i32 GetSelectedItemSaveIndex();
  void AddSaveGameItemToList(i32 saveIndex);

  bool ParseSaveData(ch const *pszFileName, ch const *pszShortName,
                     SaveGameDescription_t &save);

 private:
  MESSAGE_FUNC(OnPanelSelected, "PanelSelected");
};

#endif  // BASESAVEGAMEDIALOG_H
