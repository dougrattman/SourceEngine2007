// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "BasePanel.h"
#include "SavegameDialog.h"

#include "base/include/windows/windows_light.h"  // FILETIME
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "filesystem.h"
#include "tier1/keyvalues.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "vstdlib/random.h"

#include "SaveGameBrowserDialog.h"

extern const char *COM_GetModDirectory(void);

CGameSavePanel::CGameSavePanel(CSaveGameBrowserDialog *parent,
                               SaveGameDescription_t *pSaveDesc,
                               bool bCommandPanel)
    : BaseClass(parent, "SaveGamePanel") {
  // Store our save description internally for reference later by our parent
  m_SaveInfo = (*pSaveDesc);
  m_bNewSavePanel = bCommandPanel;

  // Setup our main graphical elements
  m_pLevelPicBorder = SETUP_PANEL(new ImagePanel(this, "LevelPicBorder"));
  m_pLevelPic = SETUP_PANEL(new ImagePanel(this, "LevelPic"));

  // Setup our various labels
  m_pChapterTitle = new Label(this, "ChapterLabel", m_SaveInfo.szComment);
  m_pTime = new Label(this, "TimeLabel", m_SaveInfo.szFileTime);
  m_pElapsedTime = new Label(this, "ElapsedLabel", m_SaveInfo.szElapsedTime);
  m_pType = new Label(this, "TypeLabel", m_SaveInfo.szType);

  // Make sure we have a chapter description
  char *chapter_name = Q_stristr(m_SaveInfo.szComment, "chapter");
  if (chapter_name) {
    char chapter_image[SOURCE_MAX_PATH];
    sprintf_s(
        chapter_image, "chapters/%s",
        Q_strlower(chapter_name, SOURCE_ARRAYSIZE(m_SaveInfo.szComment) -
                                     (chapter_name - m_SaveInfo.szComment)));
    char *ext = strrchr(chapter_image, '_');
    if (ext) *ext = '\0';

    m_pLevelPic->SetImage(chapter_image);
  } else {
    m_pLevelPic->SetImage("ui_logo");
  }

  // Setup our basic settings
  KeyValues *kv = !GameUI().IsConsoleUI()
                      ? nullptr
                      : BasePanel()->GetConsoleControlSettings()->FindKey(
                            "SaveGamePanel.res");
  LoadControlSettings("Resource/SaveGamePanel.res", nullptr, kv);

  int px, py;
  m_pLevelPicBorder->GetPos(px, py);
  SetSize(m_pLevelPicBorder->GetWide(),
          py + m_pLevelPicBorder->GetTall() + (m_pType->GetTall() + 16));
}

CGameSavePanel::~CGameSavePanel() {
  delete m_pLevelPicBorder;
  delete m_pLevelPic;
  delete m_pChapterTitle;
  delete m_pTime;
  delete m_pElapsedTime;
  delete m_pType;
}

void CGameSavePanel::ApplySchemeSettings(IScheme *pScheme) {
  m_TextColor =
      pScheme->GetColor("NewGame.TextColor", Color(255, 255, 255, 255));
  m_FillColor =
      pScheme->GetColor("NewGame.FillColor", Color(255, 255, 255, 255));
  m_DisabledColor =
      pScheme->GetColor("NewGame.DisabledColor", Color(255, 255, 255, 4));
  m_SelectedColor =
      pScheme->GetColor("NewGame.SelectionColor", Color(255, 255, 255, 255));

  // Turn various labels off if we're the "stubbed" panel
  if (m_bNewSavePanel) {
    m_pTime->SetVisible(false);
    m_pElapsedTime->SetVisible(false);
    m_pType->SetVisible(false);
  }

  // Setup our initial state
  m_pChapterTitle->SetFgColor(m_TextColor);
  m_pTime->SetFgColor(m_TextColor);
  m_pElapsedTime->SetFgColor(m_TextColor);

  m_pLevelPic->SetFillColor(Color(0, 0, 0, 255));
  m_pLevelPicBorder->SetFillColor(Color(0, 0, 0, 255));

  if (m_bNewSavePanel) {
    float flScaleAmount = m_pLevelPic->GetScaleAmount();
    if (flScaleAmount <= 0.0f) flScaleAmount = 1.0f;

    // TBD: Draw the game logo here!
    int picWide = 64.0f * flScaleAmount;
    int picTall = 64.0f * flScaleAmount;
    int borderWide = m_pLevelPicBorder->GetWide();
    int borderTall = m_pLevelPicBorder->GetTall();
    int borderX, borderY;
    m_pLevelPicBorder->GetPos(borderX, borderY);
    m_pLevelPic->SetPos(borderX + ((borderWide - picWide) / 2),
                        borderY + ((borderTall - picTall) / 2));
    m_pLevelPic->SetFillColor(Color(0, 0, 0, 0));
  }

  BaseClass::ApplySchemeSettings(pScheme);
}

// Purpose: Overwrite the level description
// Input  : *pDesc - Description to use
void CGameSavePanel::SetDescription(SaveGameDescription_t *pDesc) {
  // Store our save description internally for reference later by our parent
  m_SaveInfo = (*pDesc);

  // Setup our main graphical elements
  m_pChapterTitle->SetText(m_SaveInfo.szComment);
  m_pTime->SetText(m_SaveInfo.szFileTime);
  m_pElapsedTime->SetText(m_SaveInfo.szElapsedTime);
  m_pType->SetText(m_SaveInfo.szType);

  // Make sure we have a chapter description
  char *chapter_name = Q_stristr(m_SaveInfo.szComment, "chapter");
  if (chapter_name) {
    char chapter_image[SOURCE_MAX_PATH];
    Q_strlower(chapter_name, SOURCE_ARRAYSIZE(m_SaveInfo.szComment) -
                                 (chapter_name - m_SaveInfo.szComment));
    sprintf_s(chapter_image, "chapters/%s", chapter_name);

    char *ext = Q_strrchr(chapter_image, '_');
    if (ext) *ext = '\0';

    m_pLevelPic->SetImage(chapter_image);
  }
}

// Purpose: new game chapter selection
CSaveGameBrowserDialog::CSaveGameBrowserDialog(vgui::Panel *parent)
    : BaseClass(parent, "SaveGameDialog"),
      m_bFilterAutosaves(false),
      m_iSelectedSave(-1),
      m_bScrolling(false),
      m_ScrollCt(0),
      m_ScrollSpeed(0.0f),
      m_ButtonPressed(SCROLL_NONE),
      m_ScrollDirection(SCROLL_NONE),
      m_nDeletedPanel(INVALID_INDEX),
      m_nAddedPanel(INVALID_INDEX),
      m_nUsedStorageSpace(0),
      m_bControlDisabled(false) {
  // Setup basic attributes
  SetDeleteSelfOnClose(true);
  SetSizeable(false);

  // Create the backer that highlights the currently selected save
  m_pCenterBg = SETUP_PANEL(new Panel(this, "CenterBG"));
  m_pCenterBg->SetPaintBackgroundType(2);
  m_pCenterBg->SetVisible(true);

  // Create our button footer
  m_pFooter = new CFooterPanel(parent, "SaveGameFooter");

  // Load our res files from the keyvalue we're holding
  KeyValues *pKeys = nullptr;
  if (GameUI().IsConsoleUI()) {
    pKeys =
        BasePanel()->GetConsoleControlSettings()->FindKey("SaveGameDialog.res");
  }

  LoadControlSettings("Resource/SaveGameDialog.res", nullptr, pKeys);
}

CSaveGameBrowserDialog::~CSaveGameBrowserDialog() {
  // Release all elements
  m_SavePanels.PurgeAndDeleteElements();

  // Kill the footer
  if (m_pFooter) {
    delete m_pFooter;
    m_pFooter = nullptr;
  }

  if (m_pCenterBg) {
    delete m_pCenterBg;
    m_pCenterBg = nullptr;
  }
}

// Purpose: Show the "No save games to display" indication label and hide all
// browsing UI
void CSaveGameBrowserDialog::ShowNoSaveGameUI(void) {
  // Show the "no save games" text
  vgui::Label *pNoSavesLabel =
      dynamic_cast<vgui::Label *>(FindChildByName("NoSavesLabel"));
  if (pNoSavesLabel) {
    if (m_bSaveGameIsCorrupt) {
      pNoSavesLabel->SetText("#GameUI_SaveGame_CorruptFile");
    } else {
      pNoSavesLabel->SetText("#GameUI_NoSaveGamesToDisplay");
    }
    pNoSavesLabel->SetVisible(true);
  }

  if (m_pCenterBg) m_pCenterBg->SetVisible(false);

  vgui::Panel *pLeftArrow = FindChildByName("LeftArrow");
  if (pLeftArrow) pLeftArrow->SetVisible(false);

  vgui::Panel *pRightArrow = FindChildByName("RightArrow");
  if (pRightArrow) pRightArrow->SetVisible(false);
}

// Purpose: Hide all "No save games" UI
void CSaveGameBrowserDialog::HideNoSaveGameUI(void) {
  // Show the "no save games" text
  vgui::Panel *pNoSavesLabel = FindChildByName("NoSavesLabel");
  if (pNoSavesLabel) pNoSavesLabel->SetVisible(false);
}

void CSaveGameBrowserDialog::LayoutPanels(void) {
  // Setup our panels depending on the mode we're in
  if (HasActivePanels()) {
    // Hide any indicators about no save games
    HideNoSaveGameUI();

    // Layout panel positions relative to the dialog center.
    int panelWidth = m_SavePanels[0]->GetWide() + 16;
    int dialogWidth = GetWide();
    m_PanelXPos[2] = (dialogWidth - panelWidth) / 2 + 8;
    m_PanelXPos[1] = m_PanelXPos[2] - panelWidth;
    m_PanelXPos[0] = m_PanelXPos[1];
    m_PanelXPos[3] = m_PanelXPos[2] + panelWidth;
    m_PanelXPos[4] = m_PanelXPos[3];

    m_PanelAlpha[0] = 0;
    m_PanelAlpha[1] = 64;
    m_PanelAlpha[2] = 255;
    m_PanelAlpha[3] = 64;
    m_PanelAlpha[4] = 0;

    int panelHeight;
    m_SavePanels[0]->GetSize(panelWidth, panelHeight);
    m_pCenterBg->SetVisible(true);
    m_pCenterBg->SetWide(panelWidth + 16);
    m_pCenterBg->SetPos(
        m_PanelXPos[2] - 8,
        m_PanelYPos[2] - (panelHeight - m_nCenterBgTallDefault) + 8);
    m_pCenterBg->SetBgColor(Color(190, 115, 0, 255));
  } else {
    // Hide anything to do with browsing the saves
    ShowNoSaveGameUI();
  }

  // Do internal cleanup to make sure we present a correct state to the user
  UpdateMenuComponents(SCROLL_NONE);
  UpdateFooterOptions();
}

// Purpose: Do a fancy slide-out when we're first displayed
void CSaveGameBrowserDialog::AnimateDialogStart(void) {
  const float flAnimInTime = 0.5f;
  const float flOffset = 0.1f;

  for (int i = 0; i < NUM_SLOTS; i++) {
    if (m_PanelIndex[i] == INVALID_INDEX) continue;

    // Start us at the "opening" position
    CGameSavePanel *panel = m_SavePanels[m_PanelIndex[i]];
    if (panel) {
      panel->SetPos(m_PanelXPos[0], m_PanelYPos[0]);
      panel->SetAlpha(m_PanelAlpha[0]);
      panel->SetVisible(true);
      panel->SetEnabled(true);
      panel->SetZPos(NUM_SLOTS - i);
    }

    // Now make them slide out where they're going
    GetAnimationController()->RunAnimationCommand(
        panel, "xpos", m_PanelXPos[i], 0, flAnimInTime + (flOffset * i),
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
    GetAnimationController()->RunAnimationCommand(
        panel, "ypos", m_PanelYPos[i], 0, flAnimInTime + (flOffset * i),
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

    // Panel alpha
    GetAnimationController()->RunAnimationCommand(
        panel, "alpha", m_PanelAlpha[i], 0, flAnimInTime + (flOffset * i),
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  }

  // Move and fade the back label
  m_pCenterBg->SetAlpha(0);
  int nX, nY;
  m_pCenterBg->GetPos(nX, nY);
  m_pCenterBg->SetPos(nX - m_pCenterBg->GetWide(), nY);
  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "xpos", nX, 0, flAnimInTime + (flOffset * 2),
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "alpha", 255, 0, (flAnimInTime + (flOffset * 2)) * 2.0f,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

  CGameSavePanel *selectedPanel = GetActivePanel();
  if (selectedPanel && selectedPanel->IsAutoSaveType()) {
    m_pCenterBg->SetTall(m_nCenterBgTallDefault + 20);
  } else {
    m_pCenterBg->SetTall(m_nCenterBgTallDefault);
  }
}

// Purpose: Do our initial layout
void CSaveGameBrowserDialog::Activate(void) {
  // Start scanning for saved games
  ScanSavedGames(m_bFilterAutosaves);

  // Finish our layout depending on what the result of the scan was
  LayoutPanels();

  // Animate the opening animation
  AnimateDialogStart();

  BaseClass::Activate();
}

// Purpose: Apply special properties of the menu
void CSaveGameBrowserDialog::ApplySettings(KeyValues *inResourceData) {
  BaseClass::ApplySettings(inResourceData);

  int ypos = inResourceData->GetInt("chapterypos", 20);
  for (int i = 0; i < NUM_SLOTS; ++i) {
    m_PanelYPos[i] = ypos;
  }

  m_nCenterBgTallDefault = inResourceData->GetInt("centerbgtall", 0);
  m_pCenterBg->SetTall(m_nCenterBgTallDefault);

  m_ScrollSpeedSlow = inResourceData->GetFloat("scrollslow", 0.0f);
  m_ScrollSpeedFast = inResourceData->GetFloat("scrollfast", 0.0f);
  SetFastScroll(false);
}

// Purpose: Apply scheme settings
void CSaveGameBrowserDialog::ApplySchemeSettings(vgui::IScheme *pScheme) {
  BaseClass::ApplySchemeSettings(pScheme);

  UpdateMenuComponents(SCROLL_NONE);
}

// Purpose: sets the correct properties for visible components
void CSaveGameBrowserDialog::UpdateMenuComponents(EScrollDirection dir) {
  // This is called prior to any scrolling, so we need to look ahead to the
  // post-scroll state
  int centerIdx = SLOT_CENTER;

  // Scroll given our direction of travel
  if (dir == SCROLL_LEFT) {
    ++centerIdx;
  } else if (dir == SCROLL_RIGHT) {
    --centerIdx;
  }

  int leftIdx = centerIdx - 1;
  int rightIdx = centerIdx + 1;

  // Update the state of the side arrows depending on whether or not we can
  // scroll that direction
  vgui::Panel *leftArrow = this->FindChildByName("LeftArrow");
  vgui::Panel *rightArrow = this->FindChildByName("RightArrow");
  if (leftArrow) {
    leftArrow->SetVisible(true);
    if (m_PanelIndex[leftIdx] != INVALID_INDEX) {
      leftArrow->SetFgColor(Color(255, 255, 255, 255));
    } else {
      leftArrow->SetFgColor(Color(128, 128, 128, 64));
    }
  }
  if (rightArrow) {
    rightArrow->SetVisible(true);
    if (m_PanelIndex[rightIdx] != INVALID_INDEX) {
      rightArrow->SetFgColor(Color(255, 255, 255, 255));
    } else {
      rightArrow->SetFgColor(Color(128, 128, 128, 64));
    }
  }
}

// Purpose: sets a chapter as selected
void CSaveGameBrowserDialog::SetSelectedSaveIndex(int index) {
  m_iSelectedSave = index;

  // If we have no panels, there's nothing to update
  if (HasActivePanels() == false) return;

  // Setup panels to the left of the selected panel
  int currIdx = index;
  for (int i = SLOT_CENTER; i >= 0 && currIdx >= 0; --i) {
    m_PanelIndex[i] = currIdx;
    --currIdx;
    InitPanelIndexForDisplay(i);
  }

  // Setup panels to the right of the selected panel
  currIdx = index + 1;
  for (int i = SLOT_CENTER + 1; i < NUM_SLOTS && currIdx < m_SavePanels.Count();
       ++i) {
    m_PanelIndex[i] = currIdx;
    ++currIdx;
    InitPanelIndexForDisplay(i);
  }

  UpdateMenuComponents(SCROLL_NONE);
}

// Purpose: Remove the currently selected animation from the list with proper
// animations
void CSaveGameBrowserDialog::RemoveActivePanel(void) {
  // Kill the current panel
  m_nDeletedPanel = m_PanelIndex[SLOT_CENTER];

  // Start our current panel fading
  CGameSavePanel *pPanel = m_SavePanels[m_nDeletedPanel];
  GetAnimationController()->RunAnimationCommand(
      pPanel, "alpha", 0, 0, m_ScrollSpeedFast,
      vgui::AnimationController::INTERPOLATOR_ACCEL);
  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "alpha", 0, 0, m_ScrollSpeedFast,
      vgui::AnimationController::INTERPOLATOR_ACCEL);
  PostMessage(this, new KeyValues("FinishDelete"), m_ScrollSpeed);
}

void CSaveGameBrowserDialog::CloseAfterSave(void) {
  OnCommand("CloseAndSelectResume");
}

void CSaveGameBrowserDialog::FinishInsert(void) {
  CGameSavePanel *panel = m_SavePanels[m_nAddedPanel];

  const float flScrollSpeed = 0.75f;

  // Run the actual movement
  GetAnimationController()->RunAnimationCommand(
      panel, "xpos", m_PanelXPos[SLOT_RIGHT], 0, flScrollSpeed,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  GetAnimationController()->RunAnimationCommand(
      panel, "ypos", m_PanelYPos[SLOT_RIGHT], 0, flScrollSpeed,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

  // Panel alpha
  GetAnimationController()->RunAnimationCommand(
      panel, "alpha", 255, 0, flScrollSpeed,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  PostMessage(this, new KeyValues("CloseAfterSave"), flScrollSpeed * 2.0f);
}

// Purpose: Insert a new panel at the desired location
void CSaveGameBrowserDialog::AnimateInsertNewPanel(
    const SaveGameDescription_t *pDesc) {
  // This is the panel that's going to move
  CGameSavePanel *pNewPanel =
      SETUP_PANEL(new CGameSavePanel(this, (SaveGameDescription_t *)pDesc));
  pNewPanel->SetVisible(false);

  // Tack this onto the list
  m_nAddedPanel = m_SavePanels.InsertAfter(0, pNewPanel);

  // Set it up but turn it off immediately
  pNewPanel->SetPos(m_PanelXPos[SLOT_CENTER], m_PanelYPos[SLOT_CENTER]);
  pNewPanel->SetVisible(true);
  pNewPanel->SetEnabled(true);
  pNewPanel->SetZPos(0);
  pNewPanel->SetAlpha(0.0f);

  // Increment our indices to reflect the change
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (m_PanelIndex[i] == INVALID_INDEX) continue;

    if (m_PanelIndex[i] > 0) {
      m_PanelIndex[i]++;
    }
  }

  // Fade the right panel away
  if (IsValidPanel(m_PanelIndex[SLOT_RIGHT])) {
    CGameSavePanel *panel = m_SavePanels[m_PanelIndex[SLOT_RIGHT]];

    // Run the actual movement
    GetAnimationController()->RunAnimationCommand(
        panel, "xpos", m_PanelXPos[SLOT_OFFRIGHT], 0, m_ScrollSpeed,
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
    GetAnimationController()->RunAnimationCommand(
        panel, "ypos", m_PanelYPos[SLOT_OFFRIGHT], 0, m_ScrollSpeed,
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

    // Panel alpha
    GetAnimationController()->RunAnimationCommand(
        panel, "alpha", m_PanelAlpha[SLOT_OFFRIGHT], 0, m_ScrollSpeed,
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

    PostMessage(this, new KeyValues("FinishInsert"), m_ScrollSpeed);
  } else {
    PostMessage(this, new KeyValues("FinishInsert"), 0.1f);
  }
}

// Purpose: Pop in the new description
void CSaveGameBrowserDialog::FinishOverwriteFadeDown(void) {
  const float flFadeInTime = 0.25f;

  // Fade the right panel away
  CGameSavePanel *pActivePanel = GetActivePanel();
  if (pActivePanel) {
    pActivePanel->SetDescription(&m_NewSaveGameDesc);

    // Panel alpha
    GetAnimationController()->RunAnimationCommand(
        pActivePanel, "alpha", m_PanelAlpha[SLOT_CENTER], 0, flFadeInTime,
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  }

  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "alpha", 255, 0, flFadeInTime,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  PostMessage(this, new KeyValues("CloseAfterSave"), flFadeInTime + 0.1f);
}

// Purpose: Animate an overwrite event by fading out the old panel and bringing
// it back with a new description Input  : *pNewDesc - The new description to
// display
void CSaveGameBrowserDialog::AnimateOverwriteActivePanel(
    const SaveGameDescription_t *pNewDesc) {
  // Save a copy of this description
  m_NewSaveGameDesc = (*pNewDesc);

  // Fade the right panel away
  CGameSavePanel *pActivePanel = GetActivePanel();
  if (pActivePanel) {
    // Panel alpha
    GetAnimationController()->RunAnimationCommand(
        pActivePanel, "alpha", 0, 0, 0.5f,
        vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  }

  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "alpha", 0, 0, 0.5f,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  PostMessage(this, new KeyValues("FinishOverwriteFadeDown"), 0.75f);
}

// Purpose: Called before a panel scroll starts.
void CSaveGameBrowserDialog::PreScroll(EScrollDirection dir) {
  int hideIdx = INVALID_INDEX;
  if (m_nDeletedPanel != INVALID_INDEX) {
    hideIdx = m_nDeletedPanel;
  } else if (dir == SCROLL_LEFT) {
    hideIdx = m_PanelIndex[SLOT_LEFT];
  } else if (dir == SCROLL_RIGHT) {
    hideIdx = m_PanelIndex[SLOT_RIGHT];
  }

  if (hideIdx != INVALID_INDEX) {
    // Push back the panel that's about to be hidden
    // so the next panel scrolls over the top of it.
    m_SavePanels[hideIdx]->SetZPos(0);
  }
}

// Purpose: Called after a panel scroll finishes.
void CSaveGameBrowserDialog::PostScroll(EScrollDirection dir) {
  // TODO(d.rattman): Nothing to do here...
}

// Purpose: Initiates a panel scroll and starts the animation.
void CSaveGameBrowserDialog::ScrollSelectionPanels(EScrollDirection dir) {
  // Only initiate a scroll if panels aren't currently scrolling
  if (!m_bScrolling) {
    // Handle any pre-scroll setup
    PreScroll(dir);

    if (dir == SCROLL_LEFT) {
      m_ScrollCt += SCROLL_LEFT;
    } else if (dir == SCROLL_RIGHT && m_PanelIndex[SLOT_CENTER] != 0) {
      m_ScrollCt += SCROLL_RIGHT;
    }

    m_bScrolling = true;
    AnimateSelectionPanels();

    // Update the arrow colors, help text, and buttons. Doing it here looks
    // better than having the components change after the entire scroll
    // animation has finished.
    UpdateMenuComponents(m_ScrollDirection);
  }
}

// Purpose: Do all slide animation work here
// Input  : nPanelIndex - Panel we're currently operating on
//			nNextPanelIndex - Panel we're going to be moving over
void CSaveGameBrowserDialog::PerformSlideAction(int nPanelIndex,
                                                int nNextPanelIndex) {
  CGameSavePanel *panel = m_SavePanels[m_PanelIndex[nPanelIndex]];

  // Run the actual movement
  GetAnimationController()->RunAnimationCommand(
      panel, "xpos", m_PanelXPos[nNextPanelIndex], 0, m_ScrollSpeed,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
  GetAnimationController()->RunAnimationCommand(
      panel, "ypos", m_PanelYPos[nNextPanelIndex], 0, m_ScrollSpeed,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

  // Panel alpha
  GetAnimationController()->RunAnimationCommand(
      panel, "alpha", m_PanelAlpha[nNextPanelIndex], 0, m_ScrollSpeed,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);
}

// Purpose: Initiates the scripted scroll and fade effects of all five slotted
// panels
void CSaveGameBrowserDialog::AnimateSelectionPanels(void) {
  int idxOffset = 0;
  int startIdx = SLOT_LEFT;
  int endIdx = SLOT_RIGHT;

  // Don't scroll outside the bounds of the panel list
  if (m_ScrollCt >= SCROLL_LEFT &&
      m_PanelIndex[SLOT_CENTER] < m_SavePanels.Count() - 1) {
    if (m_nDeletedPanel != INVALID_INDEX) {
      startIdx = SLOT_RIGHT;
    }

    idxOffset = -1;
    endIdx = SLOT_OFFRIGHT;
    m_ScrollDirection = SCROLL_LEFT;
  } else if (m_ScrollCt <= SCROLL_RIGHT && m_PanelIndex[SLOT_CENTER] > 0) {
    idxOffset = 1;
    startIdx = SLOT_OFFLEFT;
    m_ScrollDirection = SCROLL_RIGHT;
  }

  if (0 == idxOffset) {
    // Kill the scroll, it's outside the bounds
    m_ScrollCt = 0;
    m_bScrolling = false;
    m_ScrollDirection = SCROLL_NONE;
    vgui::surface()->PlaySound("player/suit_denydevice.wav");
    return;
  }

  // Should never happen
  if (startIdx > endIdx) return;

  for (int i = startIdx; i <= endIdx; ++i) {
    // Don't animate the special panel, just skip it
    if (m_PanelIndex[i] == m_nDeletedPanel) continue;

    int nNextIdx = i + idxOffset;
    if (m_PanelIndex[i] != INVALID_INDEX) {
      PerformSlideAction(i, nNextIdx);
    }
  }

  vgui::surface()->PlaySound("UI/buttonclick.wav");

  // Animate the center background panel
  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "alpha", 0, 0, m_ScrollSpeed * 0.25f,
      vgui::AnimationController::INTERPOLATOR_SIMPLESPLINE);

  // Scrolling up through chapters, offset is negative
  m_iSelectedSave -= idxOffset;

  UpdateFooterOptions();

  PostMessage(this, new KeyValues("FinishScroll"), m_ScrollSpeed);
}

// Purpose: After a scroll, each panel slot holds the index of a panel that has
//			scrolled to an adjacent slot. This function updates each
// slot so 			it holds the index of the panel that is actually
// in that slot's position.
void CSaveGameBrowserDialog::ShiftPanelIndices(int offset) {
  // Shift all the elements over one slot, then calculate what the last slot's
  // index should be.
  int lastSlot = NUM_SLOTS - 1;

  // Handle the deletion case
  if (m_nDeletedPanel != INVALID_INDEX) {
    // Scroll panels in from the right
    Q_memmove(&m_PanelIndex[SLOT_CENTER], &m_PanelIndex[SLOT_RIGHT],
              2 * sizeof(m_PanelIndex[SLOT_CENTER]));

    if (m_PanelIndex[lastSlot] != INVALID_INDEX) {
      int num = m_PanelIndex[lastSlot] + 1;
      if (IsValidPanel(num)) {
        m_PanelIndex[lastSlot] = num;
        InitPanelIndexForDisplay(lastSlot);
      } else {
        m_PanelIndex[lastSlot] = INVALID_INDEX;
      }
    }
  } else if (offset > 0) {
    // Hide the panel that's dropping out of the slots
    if (IsValidPanel(m_PanelIndex[0])) {
      m_SavePanels[m_PanelIndex[0]]->SetVisible(false);
    }

    // Scrolled panels to the right, so shift the indices one slot to the left
    Q_memmove(&m_PanelIndex[0], &m_PanelIndex[1],
              lastSlot * sizeof(m_PanelIndex[0]));
    if (m_PanelIndex[lastSlot] != INVALID_INDEX) {
      int num = m_PanelIndex[lastSlot] + 1;
      if (IsValidPanel(num)) {
        m_PanelIndex[lastSlot] = num;
        InitPanelIndexForDisplay(lastSlot);
      } else {
        m_PanelIndex[lastSlot] = INVALID_INDEX;
      }
    }
  } else {
    // Hide the panel that's dropping out of the slots
    if (IsValidPanel(m_PanelIndex[lastSlot])) {
      m_SavePanels[m_PanelIndex[lastSlot]]->SetVisible(false);
    }

    // Scrolled panels to the left, so shift the indices one slot to the right
    Q_memmove(&m_PanelIndex[1], &m_PanelIndex[0],
              lastSlot * sizeof(m_PanelIndex[0]));
    if (m_PanelIndex[0] != INVALID_INDEX) {
      int num = m_PanelIndex[0] - 1;
      if (IsValidPanel(num)) {
        m_PanelIndex[0] = num;
        InitPanelIndexForDisplay(0);
      } else {
        m_PanelIndex[0] = INVALID_INDEX;
      }
    }
  }
}

// Purpose: Validates an index into the selection panels vector
bool CSaveGameBrowserDialog::IsValidPanel(const int idx) {
  if (idx < 0 || idx >= m_SavePanels.Count()) return false;

  return true;
}

// Purpose: Sets up a panel's properties before it is displayed
void CSaveGameBrowserDialog::InitPanelIndexForDisplay(const int idx) {
  CGameSavePanel *panel = m_SavePanels[m_PanelIndex[idx]];
  if (panel) {
    panel->SetPos(m_PanelXPos[idx], m_PanelYPos[idx]);
    panel->SetAlpha(m_PanelAlpha[idx]);
    panel->SetVisible(true);
    panel->SetEnabled(true);
    if (m_PanelAlpha[idx]) {
      panel->SetZPos(NUM_SLOTS);
    }
  }
}

// Purpose: Sets which scroll speed should be used
void CSaveGameBrowserDialog::SetFastScroll(bool fast) {
  m_ScrollSpeed = fast ? m_ScrollSpeedFast : m_ScrollSpeedSlow;
}

// Purpose: Checks if a button is being held down, and speeds up the scroll
void CSaveGameBrowserDialog::ContinueScrolling(void) {
  if (!GameUI().IsConsoleUI()) {
    if (m_PanelIndex[SLOT_CENTER - 1] % 3) {
      ScrollSelectionPanels(m_ScrollDirection);
    }
    return;
  }

  if (m_ButtonPressed == m_ScrollDirection) {
    SetFastScroll(true);
    ScrollSelectionPanels(m_ScrollDirection);
  } else if (m_ButtonPressed != SCROLL_NONE) {
    // The other direction has been pressed - start a slow scroll
    SetFastScroll(false);
    ScrollSelectionPanels((EScrollDirection)m_ButtonPressed);
  } else {
    SetFastScroll(false);
  }
}

// Purpose: Fade animation has finished, now slide or be done
void CSaveGameBrowserDialog::FinishDelete(void) {
  // Catch the case where all saves are now gone!
  if (m_SavePanels.Count() == 1) {
    m_nDeletedPanel = INVALID_INDEX;
    m_SavePanels.PurgeAndDeleteElements();

    for (int i = 0; i < NUM_SLOTS; i++) {
      m_PanelIndex[i] = INVALID_INDEX;
    }

    LayoutPanels();
    return;
  }

  EScrollDirection nDirection =
      (IsValidPanel(m_nDeletedPanel + 1)) ? SCROLL_LEFT : SCROLL_RIGHT;
  ScrollSelectionPanels(nDirection);
}

// Purpose: Called when a scroll distance of one slot has been completed
void CSaveGameBrowserDialog::FinishScroll(void) {
  // Fade the center bg panel back in
  GetAnimationController()->RunAnimationCommand(
      m_pCenterBg, "alpha", 255, 0, m_ScrollSpeed * 0.25f,
      vgui::AnimationController::INTERPOLATOR_LINEAR);

  ShiftPanelIndices(m_ScrollDirection);
  m_bScrolling = false;
  m_ScrollCt = 0;

  // End of scroll step
  PostScroll(m_ScrollDirection);

  if (m_nDeletedPanel != INVALID_INDEX) {
    // Find where we're going next
    int newSave = m_nDeletedPanel;
    if (m_SavePanels.IsValidIndex(m_nDeletedPanel + 1) == false) {
      newSave = m_nDeletedPanel - 1;
    }

    // Remove it from our list
    CGameSavePanel *pPanel = m_SavePanels[m_nDeletedPanel];
    m_SavePanels.Remove(m_nDeletedPanel);
    delete pPanel;

    // Decrement all the indices to reflect the change
    for (int i = 0; i < NUM_SLOTS; i++) {
      if (m_PanelIndex[i] > m_nDeletedPanel) m_PanelIndex[i]--;
    }

    // Clear the spot and be done with it
    SetSelectedSaveIndex(newSave);
    m_nDeletedPanel = INVALID_INDEX;
    UpdateMenuComponents(SCROLL_NONE);
  }

  // Size the "autosave" blade if need-be
  CGameSavePanel *selectedPanel = GetActivePanel();
  if (selectedPanel && selectedPanel->IsAutoSaveType()) {
    m_pCenterBg->SetTall(m_nCenterBgTallDefault + 20);
  } else {
    m_pCenterBg->SetTall(m_nCenterBgTallDefault);
  }

  // Continue scrolling if necessary
  ContinueScrolling();
}

void CSaveGameBrowserDialog::OnClose(void) {
  SetControlDisabled(true);

  m_KeyRepeat.Reset();
  BasePanel()->RunCloseAnimation("CloseNewGameDialog_OpenMainMenu");

  BaseClass::OnClose();
}

// Purpose: Our save games have changed, so layout our panel again
void CSaveGameBrowserDialog::RefreshSaveGames(void) {
  // Close any pending messages
  BasePanel()->CloseMessageDialog(DIALOG_STACK_IDX_WARNING);

  // Don't leave us in a locked state
  SetControlDisabled(false);

  // Re-scan the saved games
  ScanSavedGames(m_bFilterAutosaves);

  // Re-layout the panels
  LayoutPanels();

  // Run our animation again
  AnimateDialogStart();
}

void CSaveGameBrowserDialog::PerformSelectedAction(void) {
  // By default, do nothing
  m_KeyRepeat.Reset();
}

void CSaveGameBrowserDialog::PerformDeletion(void) {
  // By default, do nothing
  m_KeyRepeat.Reset();
}

// Purpose: Release our key repeater
void CSaveGameBrowserDialog::OnKeyCodeReleased(vgui::KeyCode code) {
  m_KeyRepeat.KeyUp(code);

  BaseClass::OnKeyCodeReleased(code);
}

// Purpose: Update our keypress repeater
void CSaveGameBrowserDialog::OnThink(void) {
  vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
  if (code) {
    OnKeyCodePressed(code);
  }

  BaseClass::OnThink();
}

void CSaveGameBrowserDialog::OnKeyCodePressed(vgui::KeyCode code) {
  // If the console has UI up, then ignore it
  if (BasePanel()->IsWaitingForConsoleUI()) return;

  // Inhibit key activity during transitions
  if (GetAlpha() != 255 || m_bControlDisabled) return;

  m_KeyRepeat.KeyDown(code);

  switch (code) {
    case KEY_XBUTTON_A:
      PerformSelectedAction();
      break;

    case KEY_XBUTTON_B:
      OnClose();
      break;

    case KEY_XBUTTON_X:
      PerformDeletion();
      break;

    case KEY_XBUTTON_Y:
      BasePanel()->OnChangeStorageDevice();
      break;

      // Move the selection up and down
    case KEY_XSTICK1_LEFT:
    case KEY_XBUTTON_LEFT:
      ScrollSelectionPanels(SCROLL_RIGHT);
      break;

    case KEY_XSTICK1_RIGHT:
    case KEY_XBUTTON_RIGHT:
      ScrollSelectionPanels(SCROLL_LEFT);
      break;

    default:
      break;
  }
}

void CSaveGameBrowserDialog::PaintBackground(void) {
  int wide, tall;
  GetSize(wide, tall);

  Color col = GetBgColor();
  DrawBox(0, 0, wide, tall, col, 1.0f);

  int y = 32;

  // draw an inset
  Color darkColor;
  darkColor.SetColor(0.70f * (float)col.r(), 0.70f * (float)col.g(),
                     0.70f * (float)col.b(), col.a());
  vgui::surface()->DrawSetColor(darkColor);
  vgui::surface()->DrawFilledRect(8, y, wide - 8, tall - 8);
}

// Purpose: Parses the save game info out of the .sav file header
bool CSaveGameBrowserDialog::ParseSaveData(char const *pszFileName,
                                           char const *pszShortName,
                                           SaveGameDescription_t *save) {
  if (!pszFileName || !pszShortName) return false;

  strcpy_s(save->szShortName, pszShortName);
  strcpy_s(save->szFileName, pszFileName);

  FileHandle_t fh = g_pFullFileSystem->Open(pszFileName, "rb", "MOD");
  if (fh == FILESYSTEM_INVALID_HANDLE) return false;

  save->iSize = g_pFullFileSystem->Size(fh);

  char map_name[SAVEGAME_MAPNAME_LEN], save_comment[SAVEGAME_COMMENT_LEN];
  bool is_save_ok = SaveReadNameAndComment(fh, map_name, save_comment);
  g_pFullFileSystem->Close(fh);

  if (!is_save_ok) return false;

  strcpy_s(save->szMapName, map_name);
  // Elapsed time is the last 6 characters in comment. (mmm:ss)
  size_t i = strlen(save_comment);

  char elapsed_time[SAVEGAME_ELAPSED_LEN];
  strcpy_s(elapsed_time, "??");

  if (i >= 6) {
    Q_strncpy(elapsed_time, (char *)&save_comment[i - 6], 7);
    elapsed_time[6] = '\0';

    // parse out
    int minutes = atoi(elapsed_time);
    const int seconds = atoi(elapsed_time + 4);
    const int hours = minutes / 60;
    minutes %= 60;

    wchar_t wzHours[6];
    _snwprintf_s(wzHours, SOURCE_ARRAYSIZE(wzHours), L"%d", hours);
    wchar_t wzMins[4];
    _snwprintf_s(wzMins, SOURCE_ARRAYSIZE(wzMins), L"%d", minutes);
    wchar_t wzSecs[4];
    _snwprintf_s(wzSecs, SOURCE_ARRAYSIZE(wzSecs), L"%d", seconds);

    wchar_t buf[20];

    // reformat
    if (hours) {
      g_pVGuiLocalize->ConstructString(
          buf, SOURCE_ARRAYSIZE(buf),
          g_pVGuiLocalize->Find("#GameUI_LoadDialog_Hr_Min"), 2, wzHours,
          wzMins);
    } else if (minutes) {
      g_pVGuiLocalize->ConstructString(
          buf, SOURCE_ARRAYSIZE(buf),
          g_pVGuiLocalize->Find("#GameUI_LoadDialog_Min_Sec"), 2, wzMins,
          wzSecs);
    } else {
      g_pVGuiLocalize->ConstructString(
          buf, SOURCE_ARRAYSIZE(buf),
          g_pVGuiLocalize->Find("#GameUI_LoadDialog_Sec"), 1, wzSecs);
    }

    g_pVGuiLocalize->ConvertUnicodeToANSI(buf, elapsed_time,
                                          sizeof(elapsed_time));

    // Chop elapsed out of comment.
    char *pChop = Q_stristr(save_comment, " ");
    if (pChop != nullptr) *pChop = '\0';
  }

  // calculate the file name to print
  const char *pszType = "";
  if (strstr(pszFileName, "quick")) {
    pszType = "#GameUI_QuickSave";
  } else if (strstr(pszFileName, "autosave")) {
    pszType = "#GameUI_AutoSave";
  }

  strcpy_s(save->szType, pszType);
  strcpy_s(save->szComment, save_comment);
  strcpy_s(save->szElapsedTime, elapsed_time);

  // Now get file time stamp.
  long fileTime = g_pFullFileSystem->GetFileTime(pszFileName);
  char szFileTime[32];

  g_pFullFileSystem->FileTimeToString(szFileTime, SOURCE_ARRAYSIZE(szFileTime),
                                      fileTime);
  char *newline = strstr(szFileTime, "\n");
  if (newline) *newline = '\0';

  strcpy_s(save->szFileTime, szFileTime);
  save->iTimestamp = fileTime;

  return true;
}

// Purpose: Update our footer options depending on what we've selected
void CSaveGameBrowserDialog::UpdateFooterOptions(void) {
  // Do nothing
}

// Purpose: Sort our games by time
void CSaveGameBrowserDialog::SortSaveGames(SaveGameDescription_t *pSaves,
                                           unsigned int nNumSaves) {
  qsort(pSaves, nNumSaves, sizeof(SaveGameDescription_t),
        &CBaseSaveGameDialog::SaveGameSortFunc);
}

// Purpose: builds save game list from directory
void CSaveGameBrowserDialog::ScanSavedGames(bool bIgnoreAutosave) {
  // Start with a clean slate
  m_nUsedStorageSpace = 0;
  m_bSaveGameIsCorrupt = false;

  // Clear all known panels I'm holding now
  m_SavePanels.PurgeAndDeleteElements();

  // Reset all indices
  for (int i = 0; i < NUM_SLOTS; ++i) {
    m_PanelIndex[i] = INVALID_INDEX;
  }

  // Clear our list
  CUtlVector<SaveGameDescription_t> saveGames;

  // Get the search path
  char save_dir[SOURCE_MAX_PATH];
  sprintf_s(save_dir, "save/*");

  Q_DefaultExtension(save_dir, ".sav", SOURCE_ARRAYSIZE(save_dir));
  Q_FixSlashes(save_dir);

  // iterate the saved files
  FileFindHandle_t handle;
  const char *save_file_name = g_pFullFileSystem->FindFirst(save_dir, &handle);

  while (save_file_name) {
    if (!_strnicmp(save_file_name, "HLSave", strlen("HLSave"))) {
      save_file_name = g_pFullFileSystem->FindNext(handle);
      continue;
    }

    char szFileName[SOURCE_MAX_PATH];
    sprintf_s(szFileName, "save/%s", save_file_name);
    Q_FixSlashes(szFileName);

    // Only load save games from the current mod's save dir
    if (!g_pFullFileSystem->FileExists(szFileName, "MOD")) {
      save_file_name = g_pFullFileSystem->FindNext(handle);
      continue;
    }

    SaveGameDescription_t save;
    if (ParseSaveData(szFileName, save_file_name, &save)) {
      // Add on this file's size to the count
      m_nUsedStorageSpace += save.iSize;

      // Always ignore autosave dangerous (they're not considered safe until
      // committed)
      if (Q_stristr(save.szShortName, "dangerous")) {
        save_file_name = g_pFullFileSystem->FindNext(handle);
        continue;
      }

      // If we're ignoring autosaves, skip it here
      if (bIgnoreAutosave) {
        if (!_stricmp(save.szType, "#GameUI_Autosave")) {
          save_file_name = g_pFullFileSystem->FindNext(handle);
          continue;
        }
      }

      saveGames.AddToTail(save);
    }

    save_file_name = g_pFullFileSystem->FindNext(handle);
  }

  g_pFullFileSystem->FindClose(handle);

  // Sort the save list
  SortSaveGames(saveGames.Base(), saveGames.Count());

  // Now add them in order
  for (int i = 0; i < saveGames.Count(); i++) {
    CGameSavePanel *savePanel =
        SETUP_PANEL(new CGameSavePanel(this, &saveGames[i]));

    savePanel->SetVisible(false);
    m_SavePanels.AddToTail(savePanel);
  }

  // Notify derived classes that save games are done being scanned
  OnDoneScanningSaveGames();

  // Always start with the first panel (as we're sorted in a specific order)
  SetSelectedSaveIndex(0);
}

// Purpose: Return the currently selected panel

CGameSavePanel *CSaveGameBrowserDialog::GetActivePanel(void) {
  if (IsValidPanel(m_iSelectedSave) == false) return nullptr;

  return m_SavePanels[m_iSelectedSave];
}
