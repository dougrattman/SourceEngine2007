// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "stdafx.h"

#include <wincon.h>
#include "ProcessWnd.h"
#include "hammer.h"

#include "tier0/include/memdbgon.h"

#define IDC_PROCESSWND_EDIT 1
#define IDC_PROCESSWND_COPYALL 2

BEGIN_MESSAGE_MAP(CProcessWnd, CWnd)
ON_BN_CLICKED(IDC_PROCESSWND_COPYALL, OnCopyAll)
//{{AFX_MSG_MAP(CProcessWnd)
ON_BN_CLICKED(IDC_PROCESSWND_COPYALL, OnCopyAll)
ON_WM_TIMER()
ON_WM_CREATE()
ON_WM_SIZE()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

LPCTSTR GetErrorString();

CProcessWnd::CProcessWnd() { Font.CreatePointFont(90, "Courier New"); }

CProcessWnd::~CProcessWnd() {}

int CProcessWnd::Execute(LPCTSTR command, ...) {
  CString buffer;

  va_list vl;
  va_start(vl, command);

  while (true) {
    char* p = va_arg(vl, char*);
    if (!p) break;

    buffer += p;
    buffer += " ";
  }

  va_end(vl);

  return Execute(command, buffer.operator LPCSTR());
}

void CProcessWnd::Clear() {
  m_EditText.Empty();
  Edit.SetWindowText("");
  Edit.RedrawWindow();
}

void CProcessWnd::Append(CString message) {
  m_EditText += message;

  Edit.SetWindowText(m_EditText);
  Edit.LineScroll(Edit.GetLineCount());
  Edit.RedrawWindow();
}

int CProcessWnd::Execute(LPCTSTR cmd, LPCTSTR cmd_line) {
  int rval = -1;
  HANDLE cmd_stdin_read_pipe, cmd_stdin_write_pipe, cmd_stdout_read_pipe,
      cmd_stdout_write_pipe, cmd_stderr_write_pipe;

  // Set the bInheritHandle flag so pipe handles are inherited.
  SECURITY_ATTRIBUTES security_attrs = {sizeof(SECURITY_ATTRIBUTES)};
  security_attrs.bInheritHandle = TRUE;
  security_attrs.lpSecurityDescriptor = nullptr;

  // Create a pipe for the child's STDOUT.
  if (CreatePipe(&cmd_stdout_read_pipe, &cmd_stdout_write_pipe, &security_attrs,
                 0)) {
    if (CreatePipe(&cmd_stdin_read_pipe, &cmd_stdin_write_pipe, &security_attrs,
                   0)) {
      if (DuplicateHandle(GetCurrentProcess(), cmd_stdout_write_pipe,
                          GetCurrentProcess(), &cmd_stderr_write_pipe, 0, TRUE,
                          DUPLICATE_SAME_ACCESS)) {
        /* Now create the child process. */
        STARTUPINFO si = {sizeof si};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = cmd_stdin_read_pipe;
        si.hStdError = cmd_stderr_write_pipe;
        si.hStdOutput = cmd_stdout_write_pipe;

        PROCESS_INFORMATION pi;
        CString process;
        process.Format("%s %s", cmd, cmd_line);
        if (CreateProcess(nullptr, (char*)process.operator LPCSTR(), nullptr,
                          nullptr, TRUE, DETACHED_PROCESS, nullptr, nullptr,
                          &si, &pi)) {
          HANDLE process = pi.hProcess;

          // read from pipe..
          char buffer[4096];
          BOOL is_done = FALSE;

          while (true) {
            DWORD dwCount = 0, dwRead = 0;

            // read from input handle
            PeekNamedPipe(cmd_stdout_read_pipe, nullptr, 0, nullptr, &dwCount,
                          nullptr);

            if (dwCount) {
              dwCount = std::min(dwCount, (DWORD)SOURCE_ARRAYSIZE(buffer) - 1);
              ReadFile(cmd_stdout_read_pipe, buffer, dwCount, &dwRead, nullptr);
            }

            if (dwRead) {
              buffer[dwRead] = '\0';
              Append(buffer);
            } else if (WaitForSingleObject(process, 1000) != WAIT_TIMEOUT) {
              // check process termination
              if (is_done) break;

              is_done = TRUE;  // next time we get it
            }
          }
          rval = 0;
        } else {
          SetForegroundWindow();
          CString message;
          message.Format("> Could not execute the command:\r\n   %s\r\n",
                         process.GetString());
          Append(message);
          message.Format("> Error: \"%s\"\r\n", GetErrorString());
          Append(message);
        }

        CloseHandle(cmd_stderr_write_pipe);
      }
      CloseHandle(cmd_stdin_read_pipe);
      CloseHandle(cmd_stdin_write_pipe);
    }
    CloseHandle(cmd_stdout_read_pipe);
    CloseHandle(cmd_stdout_write_pipe);
  }

  return rval;
}

void CProcessWnd::OnTimer(UINT nIDEvent) { CWnd::OnTimer(nIDEvent); }

int CProcessWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CWnd::OnCreate(lpCreateStruct) == -1) return -1;

  // create big CEdit in window
  CRect rctClient;
  GetClientRect(rctClient);

  CRect rctEdit;
  rctEdit = rctClient;
  rctEdit.bottom = rctClient.bottom - 20;

  Edit.Create(WS_CHILD | WS_BORDER | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                  ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
              rctClient, this, IDC_PROCESSWND_EDIT);
  Edit.SetReadOnly(TRUE);
  Edit.SetFont(&Font);

  CRect rctButton;
  rctButton = rctClient;
  rctButton.top = rctClient.bottom - 20;

  m_btnCopyAll.Create("Copy to Clipboard", WS_CHILD | WS_VISIBLE, rctButton,
                      this, IDC_PROCESSWND_COPYALL);
  m_btnCopyAll.SetButtonStyle(BS_PUSHBUTTON);

  return 0;
}

void CProcessWnd::OnSize(UINT nType, int cx, int cy) {
  CWnd::OnSize(nType, cx, cy);

  // create big CEdit in window
  CRect rctClient;
  GetClientRect(rctClient);

  CRect rctEdit;
  rctEdit = rctClient;
  rctEdit.bottom = rctClient.bottom - 20;
  Edit.MoveWindow(rctEdit);

  CRect rctButton;
  rctButton = rctClient;
  rctButton.top = rctClient.bottom - 20;
  m_btnCopyAll.MoveWindow(rctButton);
}

// Purpose: Prepare the process window for display. If it has not been created
//			yet, register the class and create it.
void CProcessWnd::GetReady() {
  if (!IsWindow(m_hWnd)) {
    CString strClass =
        AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW),
                            HBRUSH(GetStockObject(WHITE_BRUSH)));
    CreateEx(0, strClass, "Compile Process Window", WS_OVERLAPPEDWINDOW, 50, 50,
             600, 400, AfxGetMainWnd()->GetSafeHwnd(), HMENU(nullptr));
  }

  ShowWindow(SW_SHOW);
  SetActiveWindow();
  Clear();
}

BOOL CProcessWnd::PreTranslateMessage(MSG* pMsg) {
  // The edit control won't get keyboard commands from the window without this
  // (at least in Win2k) The right mouse context menu still will not work in w2k
  // for some reason either, although it is getting the CONTEXTMENU message (as
  // seen in Spy++)
  ::TranslateMessage(pMsg);
  ::DispatchMessage(pMsg);
  return TRUE;
}

static void CopyToClipboard(const CString& text) {
  if (OpenClipboard(nullptr)) {
    if (EmptyClipboard()) {
      HGLOBAL copy_mem =
          GlobalAlloc(GMEM_DDESHARE, text.GetLength() + sizeof(TCHAR));

      if (copy_mem != nullptr) {
        LPTSTR copy_message = (LPTSTR)GlobalLock(copy_mem);
        strcpy(copy_message, text.operator LPCSTR());
        GlobalUnlock(copy_mem);

        SetClipboardData(CF_TEXT, copy_mem);
      }
    }
    CloseClipboard();
  }
}

void CProcessWnd::OnCopyAll() {
  // Used to call m_Edit.SetSel(0,1); m_Edit.Copy(); m_Edit.Clear()
  // but in win9x the clipboard will only receive at most 64k of text from the
  // control
  CopyToClipboard(m_EditText);
}
