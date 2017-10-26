// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_DEDICATED_CONSOLE_TEXTCONSOLE_H_
#define SOURCE_DEDICATED_CONSOLE_TEXTCONSOLE_H_

#define MAX_CONSOLE_TEXTLEN 256
#define MAX_BUFFER_LINES 30

class CTextConsole {
 public:
  virtual ~CTextConsole(){};

  virtual bool Init();
  virtual void ShutDown();
  virtual void Print(const char* pszMsg);

  virtual void SetTitle(const char* pszTitle){};
  virtual void SetStatusLine(const char* pszStatus){};
  virtual void UpdateStatus(){};

  // Must be provided by children
  virtual void PrintRaw(const char* pszMsg, int nChars = 0) = 0;
  virtual void Echo(const char* pszMsg, int nChars = 0) = 0;
  virtual char* GetLine() = 0;
  virtual int GetWidth() = 0;

  virtual void SetVisible(bool visible);
  virtual bool IsVisible();

 protected:
  char m_szConsoleText[MAX_CONSOLE_TEXTLEN];  // console text buffer
  int m_nConsoleTextLen;                      // console textbuffer length
  int m_nCursorPosition;  // position in the current input line

  // Saved input data when scrolling back through command history
  char m_szSavedConsoleText[MAX_CONSOLE_TEXTLEN];  // console text buffer
  int m_nSavedConsoleTextLen;                      // console textbuffer length

  char m_aszLineBuffer[MAX_BUFFER_LINES]
                      [MAX_CONSOLE_TEXTLEN];  // command buffer last
                                              // MAX_BUFFER_LINES commands
  int m_nInputLine;                           // Current line being entered
  int m_nBrowseLine;  // current buffer line for up/down arrow
  int m_nTotalLines;  // # of nonempty lines in the buffer

  bool m_ConsoleVisible;

  int ReceiveNewline();
  void ReceiveBackspace();
  void ReceiveTab();
  void ReceiveStandardChar(const char ch);
  void ReceiveUpArrow();
  void ReceiveDownArrow();
  void ReceiveLeftArrow();
  void ReceiveRightArrow();
};

#endif  // SOURCE_DEDICATED_CONSOLE_TEXTCONSOLE_H_
