// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Contains a list of files, determines their perforce status.

#include <vgui_controls/perforcefileexplorer.h>

#include <ctype.h>
#include <vgui_controls/button.h>
#include <vgui_controls/combobox.h>
#include <vgui_controls/perforcefilelist.h>
#include <vgui_controls/tooltip.h>
#include "filesystem.h"
#include "p4lib/ip4.h"
#include "tier1/keyvalues.h"
#include "tier2/tier2.h"
#include "vgui/isystem.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

PerforceFileExplorer::PerforceFileExplorer(Panel *pParent,
                                           const char *pPanelName)
    : BaseClass(pParent, pPanelName) {
  m_pFileList = new PerforceFileList(this, "PerforceFileList");

  // Get the list of available drives and put them in a menu here.
  // Start with the directory we are in.
  m_pFullPathCombo = new ComboBox(this, "FullPathCombo", 8, false);
  m_pFullPathCombo->GetTooltip()->SetTooltipFormatToSingleLine();

  char pFullPath[SOURCE_MAX_PATH];
  g_pFullFileSystem->GetCurrentDirectory(pFullPath, sizeof(pFullPath));
  SetCurrentDirectory(pFullPath);

  m_pFullPathCombo->AddActionSignalTarget(this);

  m_pFolderUpButton = new Button(this, "FolderUpButton", "", this);
  m_pFolderUpButton->GetTooltip()->SetText("#FileOpenDialog_ToolTip_Up");
  m_pFolderUpButton->SetCommand(new KeyValues("FolderUp"));
}

PerforceFileExplorer::~PerforceFileExplorer() {}

void PerforceFileExplorer::ApplySchemeSettings(IScheme *pScheme) {
  BaseClass::ApplySchemeSettings(pScheme);
  m_pFolderUpButton->AddImage(
      scheme()->GetImage("resource/icon_folderup", false), -3);
}

void PerforceFileExplorer::PerformLayout() {
  BaseClass::PerformLayout();

  int x, y, w, h;
  GetClientArea(x, y, w, h);

  m_pFullPathCombo->SetBounds(x, y + 6, w - 30, 24);
  m_pFolderUpButton->SetBounds(x + w - 24, y + 6, 24, 24);

  m_pFileList->SetBounds(x, y + 36, w, h - 36);
}

// Sets the current directory
void PerforceFileExplorer::SetCurrentDirectory(const char *pFullPath) {
  if (!pFullPath) return;

  while (isspace(*pFullPath)) {
    ++pFullPath;
  }

  if (!pFullPath[0]) return;

  m_CurrentDirectory = pFullPath;
  m_CurrentDirectory.StripTrailingSlash();
  Q_FixSlashes(m_CurrentDirectory.Get());

  PopulateFileList();
  PopulateDriveList();

  char pCurrentDirectory[SOURCE_MAX_PATH];
  m_pFullPathCombo->GetText(pCurrentDirectory, sizeof(pCurrentDirectory));
  if (_stricmp(m_CurrentDirectory.Get(), pCurrentDirectory)) {
    char pNewDirectory[SOURCE_MAX_PATH];
    sprintf_s(pNewDirectory, "%s\\", m_CurrentDirectory.Get());

    m_pFullPathCombo->SetText(pNewDirectory);
    m_pFullPathCombo->GetTooltip()->SetText(pNewDirectory);
  }
}

void PerforceFileExplorer::PopulateDriveList() {
  char pFullPath[SOURCE_MAX_PATH * 4];
  char pSubDirPath[SOURCE_MAX_PATH * 4];
  strcpy_s(pFullPath, m_CurrentDirectory.Get());
  strcpy_s(pSubDirPath, m_CurrentDirectory.Get());

  m_pFullPathCombo->DeleteAllItems();

  // populate the drive list
  char buf[512];
  int len = system()->GetAvailableDrives(buf, 512);
  char *pBuf = buf;
  for (int i = 0; i < len / 4; i++) {
    m_pFullPathCombo->AddItem(pBuf, NULL);

    // is this our drive - add all subdirectories
    if (!_strnicmp(pBuf, pFullPath, 2)) {
      int indent = 0;
      char *pData = pFullPath;
      while (*pData) {
        if (*pData == '\\') {
          if (indent > 0) {
            memset(pSubDirPath, ' ', indent);
            memcpy(pSubDirPath + indent, pFullPath, pData - pFullPath + 1);
            pSubDirPath[indent + pData - pFullPath + 1] = 0;

            m_pFullPathCombo->AddItem(pSubDirPath, NULL);
          }
          indent += 2;
        }
        pData++;
      }
    }
    pBuf += 4;
  }
}

// Purpose: Fill the filelist with the names of all the files in the current
// directory
void PerforceFileExplorer::PopulateFileList() {
  // clear the current list
  m_pFileList->RemoveAllFiles();

  // Create filter string
  char pFullFoundPath[SOURCE_MAX_PATH];
  char pFilter[SOURCE_MAX_PATH + 3];
  sprintf_s(pFilter, "%s\\*.*", m_CurrentDirectory.Get());

  // Find all files on disk
  FileFindHandle_t h;
  const char *pFileName = g_pFullFileSystem->FindFirstEx(pFilter, NULL, &h);
  for (; pFileName; pFileName = g_pFullFileSystem->FindNext(h)) {
    if (!_stricmp(pFileName, "..") || !Q_stricmp(pFileName, ".")) continue;

    if (!Q_IsAbsolutePath(pFileName)) {
      sprintf_s(pFullFoundPath, "%s\\%s", m_CurrentDirectory.Get(), pFileName);
      pFileName = pFullFoundPath;
    }

    int nItemID = m_pFileList->AddFile(pFileName, true);
    m_pFileList->RefreshPerforceState(nItemID, true, NULL);
  }
  g_pFullFileSystem->FindClose(h);

  // Now find all files in perforce
  CUtlVector<P4File_t> &fileList = p4->GetFileList(m_CurrentDirectory);
  int nCount = fileList.Count();
  for (int i = 0; i < nCount; ++i) {
    const char *file_name = p4->String(fileList[i].m_sLocalFile);
    if (!file_name[0]) continue;

    int nItemID = m_pFileList->FindFile(file_name);
    bool bFileExists = true;
    if (nItemID == m_pFileList->InvalidItemID()) {
      // If it didn't find it, the file must not exist
      // since it already would have added it above
      bFileExists = false;
      nItemID = m_pFileList->AddFile(file_name, false, fileList[i].m_bDir);
    }
    m_pFileList->RefreshPerforceState(nItemID, bFileExists, &fileList[i]);
  }

  m_pFileList->SortList();
}

// Purpose: Handle an item in the Drive combo box being selected
void PerforceFileExplorer::OnTextChanged(KeyValues *kv) {
  Panel *pPanel = (Panel *)kv->GetPtr("panel", NULL);

  // first check which control had its text changed!
  if (pPanel == m_pFullPathCombo) {
    char pCurrentDirectory[SOURCE_MAX_PATH];
    m_pFullPathCombo->GetText(pCurrentDirectory, sizeof(pCurrentDirectory));
    SetCurrentDirectory(pCurrentDirectory);
    return;
  }
}

// Called when the file list was doubleclicked
void PerforceFileExplorer::OnItemDoubleClicked() {
  if (m_pFileList->GetSelectedItemsCount() != 1) return;

  int nItemID = m_pFileList->GetSelectedItem(0);
  if (m_pFileList->IsDirectoryItem(nItemID)) {
    const char *pDirectoryName = m_pFileList->GetFile(nItemID);
    SetCurrentDirectory(pDirectoryName);
  }
}

// Called when the folder up button was hit
void PerforceFileExplorer::OnFolderUp() {
  char pUpDirectory[SOURCE_MAX_PATH];
  strcpy_s(pUpDirectory, m_CurrentDirectory.Get());
  Q_StripLastDir(pUpDirectory, sizeof(pUpDirectory));
  Q_StripTrailingSlash(pUpDirectory);

  // This occurs at the root directory
  if (!_stricmp(pUpDirectory, ".")) return;
  SetCurrentDirectory(pUpDirectory);
}
