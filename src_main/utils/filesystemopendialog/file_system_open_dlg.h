// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef FILE_SYSTEM_OPEN_DLG_H_
#define FILE_SYSTEM_OPEN_DLG_H_

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000
// FileSystemOpenDlg.h : header file
//

#include "filesystem.h"
#include "resource.h"
#include "tier1/utlvector.h"

// CFileSystemOpenDlg dialog
struct WindowAnchor {
  CWnd* window;
  int side;          // 0=left, 1=top, 2=right, 3=bottom
  int parentSide;    // which side to anchor to parent
  int originalDist;  // original distance from the parent side
};

struct FileInfo {
  FileInfo();
  ~FileInfo();

  bool isDirectory;
  CString fileName;
  CBitmap* bitmap;
};

class FileSystemOpenDlg : public CDialog {
  friend class FileSystemOpenDialog;

  // Construction
 public:
  FileSystemOpenDlg(CreateInterfaceFn factory, CWnd* pParent = nullptr);

  // Dialog Data
  //{{AFX_DATA(CFileSystemOpenDlg)
  enum { IDD = IDD_FILESYSTEM_OPENDIALOG };
  CEdit m_FilenameLabel;
  CEdit m_FilenameControl;
  CEdit m_LookInLabel;
  CListCtrl m_FileList;
  //}}AFX_DATA

  void AddFileMask(const char* pMask);

  void SetInitialDir(const char* pDir, const char* pPathID = NULL);

  void SetFilterMdlAndJpgFiles(bool bFilter);
  CString GetFilename() const;  // Get the filename they chose.

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFileSystemOpenDlg)
 public:
  virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                      DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                      UINT nID, CCreateContext* pContext = NULL);

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);  // DDX/DDV support
                                                    //}}AFX_VIRTUAL

 private:
  enum GetEntriesMode_t { GETENTRIES_FILES_ONLY, GETENTRIES_DIRECTORIES_ONLY };
  void GetEntries(const char* pMask, CUtlVector<CString>& entries,
                  GetEntriesMode_t mode);
  void PopulateListControl();
  int SetupLabelImage(FileInfo* pInfo, CString name, bool bIsDir);

  void AddAnchor(int iDlgItem, int iSide, int anchorSide);
  void ProcessAnchor(WindowAnchor* pAnchor);

  // Implementation
 protected:
  const char* GetPathID();

  friend int CALLBACK FileListSortCallback(LPARAM lParam1, LPARAM lParam2,
                                           LPARAM lParamSort);
  friend class CFilenameShortcut;

  CUtlVector<WindowAnchor> window_anchors_;

  enum { PREVIEW_IMAGE_SIZE = 96 };

  IFileSystem* file_system_;

  // These are indexed by the lParam or userdata of each item in m_FileList.
  CUtlVector<FileInfo> m_FileInfos;

  int label_folder_, label_mdl_, label_file_;
  CBitmap bitmap_folder_, bitmap_mdl_, bitmap_file_;

  CImageList images_list_;
  CString current_dir_;
  CString file_name_;
  CString path_id_;
  CUtlVector<CString> file_name_masks_;

  // If this is true, then we get rid of .mdl files if there is a corresponding
  // .jpg file.
  bool enable_mdl_jpg_filter_;

  // Generated message map functions
  //{{AFX_MSG(CFileSystemOpenDlg)
  virtual void OnOK();
  virtual BOOL OnInitDialog();
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnDblclkFileList(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnUpButton();
  afx_msg void OnItemchangedFileList(NMHDR* pNMHDR, LRESULT* pResult);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !FILE_SYSTEM_OPEN_DLG_H_
