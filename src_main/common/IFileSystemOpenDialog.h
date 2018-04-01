// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IFILESYSTEMOPENDIALOG_H
#define IFILESYSTEMOPENDIALOG_H

#include "tier0/include/basetypes.h"
#include "tier1/interface.h"

class IFileSystem;

#define FILESYSTEMOPENDIALOG_VERSION "FileSystemOpenDlg003"

the_interface IFileSystemOpenDialog {
 public:
  // You must call this first to set the hwnd.
  virtual void Init(CreateInterfaceFn factory, void *parent_hwnd) = 0;

  // Call this to free the dialog.
  virtual void Release() = 0;

  // Use these to configure the dialog.
  virtual void AddFileMask(const ch *mask) = 0;
  virtual void SetInitialDir(const ch *directory,
                             const ch *path_id = nullptr) = 0;
  virtual void SetFilterMdlAndJpgFiles(bool is_filter) = 0;
  // Get the filename they choose.
  virtual void GetFilename(ch * file_name, int file_name_size) const = 0;

  // Call this to make the dialog itself. Returns true if they clicked OK and
  // false if they canceled it.
  virtual bool DoModal() = 0;

  // This uses the standard windows file open dialog.
  virtual bool DoModal_WindowsDialog() = 0;
};

#endif  // !IFILESYSTEMOPENDIALOG_H
