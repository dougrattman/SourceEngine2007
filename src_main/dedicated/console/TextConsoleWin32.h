// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Win32 interface for the TextConsole class.

#ifndef SOURCE_DEDICATED_CONSOLE_TEXTCONSOLEWIN32_H_
#define SOURCE_DEDICATED_CONSOLE_TEXTCONSOLEWIN32_H_

#ifdef _WIN32

#include "TextConsole.h"
#include "base/include/windows/windows_light.h"

class CTextConsoleWin32 : public CTextConsole {
 public:
  virtual ~CTextConsoleWin32(){};

  bool Init();
  void ShutDown(void);
  void PrintRaw(const char* pszMsz, int nChars = 0);
  void Echo(const char* pszMsz, int nChars = 0);
  char* GetLine(void);
  int GetWidth(void);
  void SetTitle(const char* pszTitle);
  void SetStatusLine(const char* pszStatus);
  void UpdateStatus(void);
  void SetColor(WORD);
  void SetVisible(bool visible);

 private:
  HANDLE hinput;   // standard input handle
  HANDLE houtput;  // standard output handle
  WORD Attrib;     // attrib colours for status bar

  char statusline[81];  // first line in console is status line
};

#endif  // _WIN32

#endif  // SOURCE_DEDICATED_CONSOLE_TEXTCONSOLEWIN32_H_
