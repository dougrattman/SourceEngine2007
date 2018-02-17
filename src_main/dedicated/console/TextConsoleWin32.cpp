// Copyright © 1996-2018, Valve Corporation, All rights reserved.

// CTextConsoleWin32.cpp: Win32 implementation of the TextConsole class.

#include "TextConsoleWin32.h"
#include "tier0/include/dbg.h"

#ifdef _WIN32

BOOL WINAPI ConsoleHandlerRoutine(DWORD CtrlType) {
  NOTE_UNUSED(CtrlType);
  return TRUE;
}

// GetConsoleHwnd() helper function from MSDN Knowledge Base Article Q124103
// needed, because HWND GetConsoleWindow(VOID) is not avaliable under
// Win95/98/ME

HWND GetConsoleHwnd() {
  HWND hwndFound;                // This is what is returned to the caller.
  char pszNewWindowTitle[1024];  // Contains fabricated WindowTitle
  char pszOldWindowTitle[1024];  // Contains original WindowTitle

  // Fetch current window title.
  GetConsoleTitle(pszOldWindowTitle, 1024);

  // Format a "unique" NewWindowTitle.
  wsprintf(pszNewWindowTitle, "%d/%d", GetTickCount(), GetCurrentProcessId());

  // Change current window title.
  SetConsoleTitle(pszNewWindowTitle);

  // Ensure window title has been updated.
  Sleep(40);

  // Look for NewWindowTitle.
  hwndFound = FindWindow(NULL, pszNewWindowTitle);

  // Restore original window title.

  SetConsoleTitle(pszOldWindowTitle);

  return hwndFound;
}

bool CTextConsoleWin32::Init(/*IBaseSystem * system*/) {
  if (!AllocConsole()) SetTitle("SOURCE DEDICATED SERVER");

  hinput = GetStdHandle(STD_INPUT_HANDLE);
  houtput = GetStdHandle(STD_OUTPUT_HANDLE);

  if (!SetConsoleCtrlHandler(&ConsoleHandlerRoutine, TRUE)) {
    Print("WARNING! TextConsole::Init: Could not attach console hook.\n");
  }

  Attrib = FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;

  SetWindowPos(GetConsoleHwnd(), HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOREPOSITION | SWP_SHOWWINDOW);

  return CTextConsole::Init(/*system */);
}

void CTextConsoleWin32::ShutDown() {
  FreeConsole();

  CTextConsole::ShutDown();
}

void CTextConsoleWin32::SetVisible(bool visible) {
  ShowWindow(GetConsoleHwnd(), visible ? SW_SHOW : SW_HIDE);
  m_ConsoleVisible = visible;
}

char *CTextConsoleWin32::GetLine() {
  while (1) {
    INPUT_RECORD recs[1024];
    unsigned long numread;
    unsigned long numevents;

    if (!GetNumberOfConsoleInputEvents(hinput, &numevents)) {
      Error("CTextConsoleWin32::GetLine: !GetNumberOfConsoleInputEvents");

      return NULL;
    }

    if (numevents <= 0) break;

    if (!ReadConsoleInput(hinput, recs, ARRAYSIZE(recs), &numread)) {
      Error("CTextConsoleWin32::GetLine: !ReadConsoleInput");

      return NULL;
    }

    if (numread == 0) return NULL;

    for (int i = 0; i < (int)numread; i++) {
      INPUT_RECORD *pRec = &recs[i];
      if (pRec->EventType != KEY_EVENT) continue;

      if (pRec->Event.KeyEvent.bKeyDown) {
        // check for cursor keys
        if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_UP) {
          ReceiveUpArrow();
        } else if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_DOWN) {
          ReceiveDownArrow();
        } else if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_LEFT) {
          ReceiveLeftArrow();
        } else if (pRec->Event.KeyEvent.wVirtualKeyCode == VK_RIGHT) {
          ReceiveRightArrow();
        } else {
          char ch;
          int nLen;

          ch = pRec->Event.KeyEvent.uChar.AsciiChar;
          switch (ch) {
            case '\r':  // Enter
              nLen = ReceiveNewline();
              if (nLen) {
                return m_szConsoleText;
              }
              break;

            case '\b':  // Backspace
              ReceiveBackspace();
              break;

            case '\t':  // TAB
              ReceiveTab();
              break;

            default:
              if ((ch >= ' ') &&
                  (ch <= '~'))  // dont' accept nonprintable chars
              {
                ReceiveStandardChar(ch);
              }
              break;
          }
        }
      }
    }
  }

  return NULL;
}

void CTextConsoleWin32::PrintRaw(const char *pszMsg, int nChars) {
  unsigned long dummy;

  if (nChars == 0) {
    WriteFile(houtput, pszMsg, strlen(pszMsg), &dummy, NULL);
  } else {
    WriteFile(houtput, pszMsg, nChars, &dummy, NULL);
  }
}

void CTextConsoleWin32::Echo(const char *pszMsg, int nChars) {
  PrintRaw(pszMsg, nChars);
}
int CTextConsoleWin32::GetWidth() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  int nWidth;

  nWidth = 0;

  if (GetConsoleScreenBufferInfo(houtput, &csbi)) {
    nWidth = csbi.dwSize.X;
  }

  if (nWidth <= 1) nWidth = 80;

  return nWidth;
}

void CTextConsoleWin32::SetStatusLine(const char *pszStatus) {
  strncpy(statusline, pszStatus, 80);
  statusline[79] = '\0';
  UpdateStatus();
}

void CTextConsoleWin32::UpdateStatus() {
  COORD coord;
  DWORD dwWritten = 0;
  WORD wAttrib[80];

  for (int i = 0; i < 80; i++) {
    wAttrib[i] = Attrib;  // FOREGROUND_GREEN | FOREGROUND_INTENSITY |
                          // BACKGROUND_INTENSITY ;
  }

  coord.X = coord.Y = 0;

  WriteConsoleOutputAttribute(houtput, wAttrib, 80, coord, &dwWritten);
  WriteConsoleOutputCharacter(houtput, statusline, 80, coord, &dwWritten);
}

void CTextConsoleWin32::SetTitle(const char *pszTitle) {
  SetConsoleTitle(pszTitle);
}

void CTextConsoleWin32::SetColor(WORD attrib) { Attrib = attrib; }

#endif  // _WIN32
