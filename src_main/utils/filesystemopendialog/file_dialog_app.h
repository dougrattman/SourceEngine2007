// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef FILE_DIALOG_APP_H_
#define FILE_DIALOG_APP_H_

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"  // main symbols

// See SteamFileDialog.cpp for the implementation of this class
class SteamFileDialogApp : public CWinApp {
 public:
  SteamFileDialogApp();

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSteamFileDialogApp)
  //}}AFX_VIRTUAL

  //{{AFX_MSG(CSteamFileDialogApp)
  // NOTE - the ClassWizard will add and remove member functions here.
  //    DO NOT EDIT what you see in these blocks of generated code !
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !FILE_DIALOG_APP_H_
