// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Serves as the base panel for the entire matchmaking UI
//

#ifndef MATCHMAKINGBASEPANEL_H
#define MATCHMAKINGBASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"
#include "const.h"
#include "utlstack.h"

enum EGameType {
  GAMETYPE_RANKED_MATCH,
  GAMETYPE_STANDARD_MATCH,
  GAMETYPE_SYSTEMLINK_MATCH,
};

//----------------------------
// CMatchmakingBasePanel
//----------------------------
class CMatchmakingBasePanel : public CBaseDialog {
  DECLARE_CLASS_SIMPLE(CMatchmakingBasePanel, CBaseDialog);

 public:
  CMatchmakingBasePanel(vgui::Panel *parent);
  ~CMatchmakingBasePanel();

  virtual void OnCommand(const char *pCommand);
  virtual void OnKeyCodePressed(vgui::KeyCode code);
  virtual void Activate();

  void SessionNotification(const int notification, const int param = 0);
  void SystemNotification(const int notification);
  void UpdatePlayerInfo(uint64_t nPlayerId, const char *pName, int nTeam,
                        uint8_t cVoiceState, int nPlayersNeeded, bool bHost);
  void SessionSearchResult(int searchIdx, void *pHostData,
                           XSESSION_SEARCHRESULT *pResult, int ping);

  void OnLevelLoadingStarted();
  void OnLevelLoadingFinished();
  void CloseGameDialogs(bool bActivateNext = true);
  void CloseAllDialogs(bool bActivateNext = true);
  void CloseBaseDialogs(void);

  void SetFooterButtons(CBaseDialog *pOwner, KeyValues *pData,
                        int nButtonGap = -1);
  void ShowFooter(bool bShown);
  void SetFooterButtonVisible(const char *pszText, bool bVisible);

  uint32_t GetGameType(void) { return m_nGameType; }

  MESSAGE_FUNC_CHARPTR(LoadMap, "LoadMap", mapname);

 private:
  void OnOpenWelcomeDialog();
  void OnOpenPauseDialog();
  void OnOpenRankingsDialog();
  void OnOpenSystemLinkDialog();
  void OnOpenPlayerMatchDialog();
  void OnOpenRankedMatchDialog();
  void OnOpenAchievementsDialog();
  void OnOpenLeaderboardDialog(const char *pResourceName);
  void OnOpenSessionOptionsDialog(const char *pResourceName);
  void OnOpenSessionLobbyDialog(const char *pResourceName);
  void OnOpenSessionBrowserDialog(const char *pResourceName);

  void LoadSessionProperties();
  bool ValidateSigninAndStorage(bool bOnlineRequired,
                                const char *pIssuingCommand);
  void CenterDialog(vgui::PHandle dlg);
  void PushDialog(vgui::DHANDLE<CBaseDialog> &hDialog);
  void PopDialog(bool bActivateNext = true);

  vgui::DHANDLE<CBaseDialog> m_hWelcomeDialog;
  vgui::DHANDLE<CBaseDialog> m_hPauseDialog;
  vgui::DHANDLE<CBaseDialog> m_hStatsDialog;
  vgui::DHANDLE<CBaseDialog> m_hRankingsDialog;
  vgui::DHANDLE<CBaseDialog> m_hLeaderboardDialog;
  vgui::DHANDLE<CBaseDialog> m_hSystemLinkDialog;
  vgui::DHANDLE<CBaseDialog> m_hPlayerMatchDialog;
  vgui::DHANDLE<CBaseDialog> m_hRankedMatchDialog;
  vgui::DHANDLE<CBaseDialog> m_hAchievementsDialog;
  vgui::DHANDLE<CBaseDialog> m_hSessionOptionsDialog;
  vgui::DHANDLE<CBaseDialog> m_hSessionLobbyDialog;
  vgui::DHANDLE<CBaseDialog> m_hSessionBrowserDialog;

  CUtlStack<vgui::DHANDLE<CBaseDialog> > m_DialogStack;

  uint32_t m_nSessionType;
  uint32_t m_nGameType;
  bool m_bPlayingOnline;
  char m_szMapLoadName[MAX_MAP_NAME];
  KeyValues *m_pSessionKeys;

  CFooterPanel *m_pFooter;
};

#endif  // MATCHMAKINGBASEPANEL_H
