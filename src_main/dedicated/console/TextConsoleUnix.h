// Copyright © 1996-2005, Valve Corporation, All rights reserved.
//
// Purpose: Unix interface for the TextConsole class.

#ifndef SOURCE_DEDICATED_CONSOLE_TEXTCONSOLEUNIX_H_
#define SOURCE_DEDICATED_CONSOLE_TEXTCONSOLEUNIX_H_

#ifndef _WIN32

#include <stdio.h>
#include <termios.h>
#include "textconsole.h"

typedef enum {
  ESCAPE_CLEAR = 0,
  ESCAPE_RECEIVED,
  ESCAPE_BRACKET_RECEIVED
} escape_sequence_t;

class CTextConsoleUnix : public CTextConsole {
 public:
  virtual ~CTextConsoleUnix(){};

  bool Init();
  void ShutDown(void);
  void PrintRaw(char *pszMsg, int nChars = 0);
  void Echo(char *pszMsg, int nChars = 0);
  char *GetLine(void);
  int GetWidth(void);

 private:
  int kbhit(void);

  bool m_bConDebug;

  struct termios termStored;
  FILE *tty;
};

#endif  // _WIN32

#endif  // SOURCE_DEDICATED_CONSOLE_TEXTCONSOLEUNIX_H_
