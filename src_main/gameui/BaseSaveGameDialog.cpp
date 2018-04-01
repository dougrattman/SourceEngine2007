// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "BaseSaveGameDialog.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include "FileSystem.h"
#include "MouseMessageForwardingPanel.h"
#include "TGAImagePanel.h"
#include "base/include/macros.h"
#include "savegame_version.h"
#include "tier1/utlbuffer.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/PanelListPanel.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

#define TGA_IMAGE_PANEL_WIDTH 180
#define TGA_IMAGE_PANEL_HEIGHT 100
#define MAX_LISTED_SAVE_GAMES 128

// Purpose: Describes the layout of a same game pic.
class CSaveGamePanel : public vgui::EditablePanel {
  DECLARE_CLASS_SIMPLE(CSaveGamePanel, vgui::EditablePanel);

 public:
  CSaveGamePanel(PanelListPanel *parent, const ch *name, i32 saveGameListItemID)
      : BaseClass(parent, name) {
    m_iSaveGameListItemID = saveGameListItemID;
    m_pParent = parent;
    m_pSaveGameImage = new CTGAImagePanel(this, "SaveGameImage");
    m_pAutoSaveImage = new ImagePanel(this, "AutoSaveImage");
    m_pSaveGameScreenshotBackground =
        new ImagePanel(this, "SaveGameScreenshotBackground");
    m_pChapterLabel = new Label(this, "ChapterLabel", "");
    m_pTypeLabel = new Label(this, "TypeLabel", "");
    m_pElapsedTimeLabel = new Label(this, "ElapsedTimeLabel", "");
    m_pFileTimeLabel = new Label(this, "FileTimeLabel", "");

    CMouseMessageForwardingPanel *panel =
        new CMouseMessageForwardingPanel(this, nullptr);
    panel->SetZPos(2);

    SetSize(200, 140);

    LoadControlSettings("resource/SaveGamePanel.res");

    m_FillColor = m_pSaveGameScreenshotBackground->GetFillColor();
  }

  void SetSaveGameInfo(SaveGameDescription_t &save) {
    // set the bitmap to display
    ch tga[SOURCE_MAX_PATH];
    strcpy_s(tga, save.szFileName);

    ch *extension = strstr(tga, ".sav");
    if (extension) {
      strcpy_s(extension, tga + SOURCE_ARRAYSIZE(tga) - extension, ".tga");
    }

    // If a TGA file exists then it is a user created savegame
    if (g_pFullFileSystem->FileExists(tga)) {
      m_pSaveGameImage->SetTGA(tga);
    } else {
      // If there is no TGA then it is either an autosave or the user TGA file
      // has been deleted
      m_pSaveGameImage->SetVisible(false);
      m_pAutoSaveImage->SetVisible(true);
      m_pAutoSaveImage->SetImage("resource\\autosave");
    }

    // set the title text
    m_pChapterLabel->SetText(save.szComment);

    // type
    SetControlString("TypeLabel", save.szType);
    SetControlString("ElapsedTimeLabel", save.szElapsedTime);
    SetControlString("FileTimeLabel", save.szFileTime);
  }

  MESSAGE_FUNC_INT(OnPanelSelected, "PanelSelected", state) {
    if (state) {
      // set the text color to be orange, and the pic border to be orange
      m_pSaveGameScreenshotBackground->SetFillColor(m_SelectedColor);
      m_pChapterLabel->SetFgColor(m_SelectedColor);
      m_pTypeLabel->SetFgColor(m_SelectedColor);
      m_pElapsedTimeLabel->SetFgColor(m_SelectedColor);
      m_pFileTimeLabel->SetFgColor(m_SelectedColor);
    } else {
      m_pSaveGameScreenshotBackground->SetFillColor(m_FillColor);
      m_pChapterLabel->SetFgColor(m_TextColor);
      m_pTypeLabel->SetFgColor(m_TextColor);
      m_pElapsedTimeLabel->SetFgColor(m_TextColor);
      m_pFileTimeLabel->SetFgColor(m_TextColor);
    }

    PostMessage(m_pParent->GetVParent(), new KeyValues("PanelSelected"));
  }

  virtual void OnMousePressed(vgui::MouseCode code) {
    m_pParent->SetSelectedPanel(this);
  }

  virtual void ApplySchemeSettings(IScheme *pScheme) {
    m_TextColor =
        pScheme->GetColor("NewGame.TextColor", Color(255, 255, 255, 255));
    m_SelectedColor =
        pScheme->GetColor("NewGame.SelectionColor", Color(255, 255, 255, 255));

    BaseClass::ApplySchemeSettings(pScheme);
  }

  virtual void OnMouseDoublePressed(vgui::MouseCode code) {
    // call the panel
    OnMousePressed(code);
    PostMessage(m_pParent->GetParent(),
                new KeyValues("Command", "command", "loadsave"));
  }

  i32 GetSaveGameListItemID() { return m_iSaveGameListItemID; }

 private:
  vgui::PanelListPanel *m_pParent;
  vgui::Label *m_pChapterLabel;
  CTGAImagePanel *m_pSaveGameImage;
  ImagePanel *m_pAutoSaveImage;

  // things to change color when the selection changes
  ImagePanel *m_pSaveGameScreenshotBackground;
  Label *m_pTypeLabel;
  Label *m_pElapsedTimeLabel;
  Label *m_pFileTimeLabel;
  Color m_TextColor, m_FillColor, m_SelectedColor;

  i32 m_iSaveGameListItemID;
};

CBaseSaveGameDialog::CBaseSaveGameDialog(vgui::Panel *parent, const ch *name)
    : BaseClass(parent, name) {
  CreateSavedGamesList();
  ScanSavedGames();

  new vgui::Button(this, "loadsave", "");
  SetControlEnabled("loadsave", false);
}

// Creates the load game display list.
void CBaseSaveGameDialog::CreateSavedGamesList() {
  m_pGameList = new vgui::PanelListPanel(this, "listpanel_loadgame");
  m_pGameList->SetFirstColumnWidth(0);
}

// Returns the save file name of the selected item.
i32 CBaseSaveGameDialog::GetSelectedItemSaveIndex() {
  CSaveGamePanel *panel =
      dynamic_cast<CSaveGamePanel *>(m_pGameList->GetSelectedPanel());
  if (panel) {
    // find the panel in the list
    for (i32 i = 0; i < m_SaveGames.Count(); i++) {
      if (i == panel->GetSaveGameListItemID()) {
        return i;
      }
    }
  }

  return m_SaveGames.InvalidIndex();
}

// Builds save game list from directory
void CBaseSaveGameDialog::ScanSavedGames() {
  // populate list box with all saved games on record:
  ch saves_regexp[SOURCE_MAX_PATH];
  sprintf_s(saves_regexp, "save/*.sav");

  // clear the current list
  m_pGameList->DeleteAllItems();
  m_SaveGames.RemoveAll();

  // iterate the saved files
  FileFindHandle_t find_save_handle;
  const ch *save_path =
      g_pFullFileSystem->FindFirst(saves_regexp, &find_save_handle);

  while (save_path) {
    if (!_strnicmp(save_path, "HLSave", SOURCE_ARRAYSIZE("HLSave") - 1)) {
      save_path = g_pFullFileSystem->FindNext(find_save_handle);
      continue;
    }

    ch save_file_path[SOURCE_MAX_PATH];
    sprintf_s(save_file_path, "save/%s", save_path);

    // Only load save games from the current mod's save dir
    if (!g_pFullFileSystem->FileExists(save_file_path, "MOD")) {
      save_path = g_pFullFileSystem->FindNext(find_save_handle);
      continue;
    }

    SaveGameDescription_t save_description;
    if (ParseSaveData(save_file_path, save_path, save_description)) {
      m_SaveGames.AddToTail(save_description);
    }

    save_path = g_pFullFileSystem->FindNext(find_save_handle);
  }

  g_pFullFileSystem->FindClose(find_save_handle);

  // notify derived classes that save games are being scanned (so they can
  // insert their own)
  OnScanningSaveGames();

  // sort the save list
  qsort(m_SaveGames.Base(), m_SaveGames.Count(), sizeof(SaveGameDescription_t),
        &SaveGameSortFunc);

  // add to the list
  for (i32 saveIndex = 0;
       saveIndex < m_SaveGames.Count() && saveIndex < MAX_LISTED_SAVE_GAMES;
       saveIndex++) {
    // add the item to the panel
    AddSaveGameItemToList(saveIndex);
  }

  // display a message if there are no save games
  if (!m_SaveGames.Count()) {
    vgui::Label *pNoSavesLabel = SETUP_PANEL(
        new Label(m_pGameList, "NoSavesLabel", "#GameUI_NoSaveGamesToDisplay"));
    pNoSavesLabel->SetTextColorState(vgui::Label::CS_DULL);
    m_pGameList->AddItem(nullptr, pNoSavesLabel);
  }

  SetControlEnabled("loadsave", false);
  SetControlEnabled("delete", false);
}

// Adds an item to the list.
void CBaseSaveGameDialog::AddSaveGameItemToList(i32 saveIndex) {
  // create the new panel and add to the list
  CSaveGamePanel *saveGamePanel =
      new CSaveGamePanel(m_pGameList, "SaveGamePanel", saveIndex);
  saveGamePanel->SetSaveGameInfo(m_SaveGames[saveIndex]);
  m_pGameList->AddItem(nullptr, saveGamePanel);
}

// Parses the save game info out of the .sav file header.
bool CBaseSaveGameDialog::ParseSaveData(ch const *save_file_name,
                                        ch const *short_save_name,
                                        SaveGameDescription_t &save) {
  if (!save_file_name || !short_save_name) return false;

  strcpy_s(save.szShortName, short_save_name);
  strcpy_s(save.szFileName, save_file_name);

  FileHandle_t fh = g_pFullFileSystem->Open(save_file_name, "rb", "MOD");
  if (fh == FILESYSTEM_INVALID_HANDLE) return false;

  ch map_name[SAVEGAME_MAPNAME_LEN];
  ch comment[SAVEGAME_COMMENT_LEN];

  bool was_read_ok = SaveReadNameAndComment(fh, map_name, comment);
  g_pFullFileSystem->Close(fh);

  if (!was_read_ok) return false;

  strcpy_s(save.szMapName, map_name);

  ch elapsed_time[SAVEGAME_ELAPSED_LEN];
  strcpy_s(elapsed_time, "??");

  // Elapsed time is the last 6 characters in comment. (mmm:ss)
  usize i = strlen(comment);

  if (i >= 6) {
    strncpy_s(elapsed_time, (ch *)&comment[i - 6], 7);
    elapsed_time[6] = '\0';

    // parse out
    i32 minutes = atoi(elapsed_time), seconds = atoi(elapsed_time + 4);

    // reformat
    if (minutes) {
      sprintf_s(elapsed_time, "%d %s %d %s", minutes,
                minutes == 1 ? "minute" : "minutes", seconds,
                seconds == 1 ? "second" : "seconds");
    } else {
      sprintf_s(elapsed_time, "%d %s", seconds,
                seconds == 1 ? "second" : "seconds");
    }

    // Chop elapsed out of comment.
    usize n = i - 6;
    comment[n] = '\0';
    n--;

    // Strip back the spaces at the end.
    while (n >= 1 && comment[n] && comment[n] == ' ') {
      comment[n--] = '\0';
    }
  }

  // calculate the file name to print
  const ch *save_type_ui_id = "";
  if (strstr(save_file_name, "quick")) {
    save_type_ui_id = "#GameUI_QuickSave";
  } else if (strstr(save_file_name, "autosave")) {
    save_type_ui_id = "#GameUI_AutoSave";
  }

  strcpy_s(save.szType, save_type_ui_id);
  strcpy_s(save.szComment, comment);
  strcpy_s(save.szElapsedTime, elapsed_time);

  // Now get file time stamp.
  long save_file_ticks = g_pFullFileSystem->GetFileTime(save_file_name);
  ch save_file_time[32];

  g_pFullFileSystem->FileTimeToString(
      save_file_time, SOURCE_ARRAYSIZE(save_file_time), save_file_ticks);

  ch *new_line = strstr(save_file_time, "\n");
  if (new_line) *new_line = '\0';

  strcpy_s(save.szFileTime, save_file_time);
  save.iTimestamp = save_file_ticks;

  return true;
}

// Timestamp sort function for savegames.
i32 CBaseSaveGameDialog::SaveGameSortFunc(const void *lhs, const void *rhs) {
  const SaveGameDescription_t *s1 = (const SaveGameDescription_t *)lhs;
  const SaveGameDescription_t *s2 = (const SaveGameDescription_t *)rhs;

  if (s1->iTimestamp < s2->iTimestamp) return 1;
  if (s1->iTimestamp > s2->iTimestamp) return -1;

  // timestamps are equal, so just sort by filename.
  return strcmp(s1->szFileName, s2->szFileName);
}

template <size_t name_size, size_t comment_size>
bool SaveReadNameAndComment(FileHandle_t f, ch (&name)[name_size],
                            ch (&comment)[comment_size]) {
  if (!f || !name_size || !comment_size) return false;

  name[0] = '\0';
  comment[0] = '\0';

  u32 tag;
  g_pFullFileSystem->Read(&tag, sizeof(tag), f);
  if (tag != MAKEID('J', 'S', 'A', 'V')) {
    Msg("Save '%s' is not tagged as JSAV, it is not a valid save.\n", name);
    return false;
  }

  u32 version;
  g_pFullFileSystem->Read(&version, sizeof(version), f);
  // Enforce version for now
  if (version != SAVEGAME_VERSION) {
    Msg("Save '%s' has version 0x%4.x, but supported save version is 0x%4.x.\n",
        name, version, SAVEGAME_VERSION);
    return false;
  }

  u32 expected_save_data_size, token_size, tokens_count;
  g_pFullFileSystem->Read(&expected_save_data_size,
                          sizeof(expected_save_data_size), f);
  // These two ints are the token list
  g_pFullFileSystem->Read(&tokens_count, sizeof(tokens_count), f);
  g_pFullFileSystem->Read(&token_size, sizeof(token_size), f);
  expected_save_data_size += token_size;

  // Sanity Check.
  if (tokens_count > 1024 * 1024 * 32) return false;
  if (token_size > 1024 * 1024 * 32) return false;

  std::unique_ptr<ch[]> save_data{
      std::make_unique<ch[]>(expected_save_data_size)};
  u32 actual_save_data_size =
      g_pFullFileSystem->Read(save_data.get(), expected_save_data_size, f);
  if (actual_save_data_size != expected_save_data_size) {
    Msg("Save '%s' has corrupted, expected %u bytes, got %u bytes.\n", name,
        expected_save_data_size, actual_save_data_size);
    return false;
  }

  ch *data{save_data.get()};
  std::unique_ptr<ch *[]> tokens_list;

  // Allocate a table for the strings, and parse the table
  if (token_size > 0) {
    tokens_list.reset(new ch *[tokens_count]);

    // Make sure the token strings pointed to by the pToken hashtable.
    for (u32 i{0};
         i < tokens_count &&
         static_cast<usize>(data - save_data.get()) < expected_save_data_size;
         i++) {
      // Point to each string in the pToken table
      tokens_list[i] = *data ? data : nullptr;
      // Find next token (after next 0)
      while (static_cast<usize>(data - save_data.get()) <
                 expected_save_data_size &&
             *data++)
        ;
    }
  } else {
    tokens_list = nullptr;
  }

  // short, short (size, index of field name)
  short field_size{*(short *)data};
  data += sizeof(short);
  ch *field_name{tokens_list[*(short *)data]};

  if (_stricmp(field_name, "GameHeader")) {
    Msg("'GameHeader' field missed in save '%s', it is not valid save.\n",
        name);
    return false;
  };

  // i32 (fieldcount)
  data += sizeof(short);
  i32 fields_count{*(i32 *)data};
  data += field_size;

  // Each field is a short (size), short (index of name), binary string of
  // "size" bytes (data)
  for (i32 i{0}; i < fields_count; i++) {
    // Data order is:
    // Size
    // szName
    // Actual Data

    field_size = *(short *)data;
    data += sizeof(short);

    field_name = tokens_list[*(short *)data];
    data += sizeof(short);

    if (!_stricmp(field_name, "comment")) {
      strncpy_s(comment, comment_size, data, field_size);
    } else if (!_stricmp(field_name, "mapName")) {
      strncpy_s(name, name_size, data, field_size);
    }

    // Move to Start of next field.
    data += field_size;
  }

  return strlen(name) > 0 && strlen(comment) > 0;
}

// Deletes an existing save game.
void CBaseSaveGameDialog::DeleteSaveGame(const ch *save_path) {
  if (!save_path || !save_path[0]) return;

  // delete the save game file
  g_pFullFileSystem->RemoveFile(save_path, "MOD");

  // delete the associated tga
  ch save_tga_path[SOURCE_MAX_PATH];
  strcpy_s(save_tga_path, save_path);

  ch *extension = strstr(save_tga_path, ".sav");
  if (extension)
    strcpy_s(extension,
             save_tga_path + SOURCE_ARRAYSIZE(save_tga_path) - extension,
             ".tga");

  g_pFullFileSystem->RemoveFile(save_tga_path, "MOD");
}

// One item has been selected.
void CBaseSaveGameDialog::OnPanelSelected() {
  SetControlEnabled("loadsave", true);
  SetControlEnabled("delete", true);
}
