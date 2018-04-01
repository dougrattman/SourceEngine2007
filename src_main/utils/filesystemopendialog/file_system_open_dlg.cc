// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "file_system_open_dlg.h"

#include "deps/libjpeg/jpeglib.h"

#include "ifilesystemopendialog.h"
#include "tier1/utldict.h"

#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

FileInfo::FileInfo() : bitmap{nullptr} {}

FileInfo::~FileInfo() {}

// This caches the thumbnail bitmaps we generate to speed up browsing.
class BitmapCache {
 public:
  BitmapCache() {
    current_memory_usage_ = 0;
    max_memory_usage_ = 1024 * 1024 * 16;
  }

  void AddToCache(CBitmap *pBitmap, const char *pName, int memoryUsage,
                  bool bLock) {
    Assert(bitmaps_.Find(pName) == -1);
    current_memory_usage_ += memoryUsage;

    BitmapCacheEntry cache_entry{pBitmap, memoryUsage, bLock};
    bitmaps_.Insert(pName, cache_entry);

    EnsureMemoryUsage();
  }

  CBitmap *Find(const char *pName) {
    const int i = bitmaps_.Find(pName);
    return i == -1 ? nullptr : bitmaps_[i].bitmap;
  }

  void UnlockAll() {
    for (int i = bitmaps_.First(); i != bitmaps_.InvalidIndex();
         i = bitmaps_.Next(i)) {
      bitmaps_[i].isLocked = false;
    }
  }

 private:
  void EnsureMemoryUsage() {
    while (current_memory_usage_ > max_memory_usage_) {
      // Free something.
      bool is_freed = false;
      for (int i = bitmaps_.First(); i != bitmaps_.InvalidIndex();
           i = bitmaps_.Next(i)) {
        if (!bitmaps_[i].isLocked) {
          delete bitmaps_[i].bitmap;
          current_memory_usage_ -= bitmaps_[i].memoryUsage;
          bitmaps_.RemoveAt(i);
          break;
        }
      }

      // Nothing left to free?
      if (!is_freed) return;
    }
  }

 private:
  struct BitmapCacheEntry {
    BitmapCacheEntry() = default;
    BitmapCacheEntry(CBitmap *bitmap_, int memoryUsage_, bool isLocked_)
        : bitmap{bitmap_}, memoryUsage{memoryUsage_}, isLocked{isLocked_} {}

    CBitmap *bitmap;
    int memoryUsage;
    bool isLocked;
  };

  CUtlDict<BitmapCacheEntry, int> bitmaps_;
  int current_memory_usage_, max_memory_usage_;
};

BitmapCache g_BitmapCache;

FileSystemOpenDlg::FileSystemOpenDlg(CreateInterfaceFn factory, CWnd *pParent)
    : CDialog(FileSystemOpenDlg::IDD, pParent),
      file_system_{
          (IFileSystem *)factory(FILESYSTEM_INTERFACE_VERSION, nullptr)},
      enable_mdl_jpg_filter_{false} {
  //{{AFX_DATA_INIT(CFileSystemOpenDlg)
  //}}AFX_DATA_INIT

  if (!file_system_) {
    Error("Unable to connect to %s!\n", FILESYSTEM_INTERFACE_VERSION);
  }
}

void FileSystemOpenDlg::DoDataExchange(CDataExchange *pDX) {
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CFileSystemOpenDlg)
  DDX_Control(pDX, IDC_FILENAME_LABEL, m_FilenameLabel);
  DDX_Control(pDX, IDC_FILENAME, m_FilenameControl);
  DDX_Control(pDX, IDC_LOOKIN, m_LookInLabel);
  DDX_Control(pDX, IDC_FILE_LIST, m_FileList);
  //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(FileSystemOpenDlg, CDialog)
//{{AFX_MSG_MAP(CFileSystemOpenDlg)
ON_WM_CREATE()
ON_WM_SIZE()
ON_NOTIFY(NM_DBLCLK, IDC_FILE_LIST, OnDblclkFileList)
ON_BN_CLICKED(IDC_UP_BUTTON, OnUpButton)
ON_NOTIFY(LVN_ITEMCHANGED, IDC_FILE_LIST, OnItemchangedFileList)
ON_WM_KEYDOWN()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void FileSystemOpenDlg::OnOK() {
  // Make sure it's a valid filename.
  CString file_name;
  m_FilenameControl.GetWindowText(file_name);

  CString file_path = current_dir_ + "\\" + file_name;

  if (file_system_->IsDirectory(file_path, GetPathID())) {
    current_dir_ = file_path;
    PopulateListControl();
  } else if (file_system_->FileExists(file_path, GetPathID())) {
    file_name_ = file_path;

    // Translate .jpg to .mdl?
    if (enable_mdl_jpg_filter_) {
      char temp_file_path[SOURCE_MAX_PATH];
      strcpy_s(temp_file_path, file_path);

      char *dot = strrchr(temp_file_path, '.');
      if (dot) {
        if (_stricmp(dot, ".jpeg") == 0 || _stricmp(dot, ".jpg") == 0) {
          strncpy_s(dot,
                    temp_file_path + SOURCE_ARRAYSIZE(temp_file_path) - dot,
                    ".mdl", 5);
          file_name_ = temp_file_path;
        }
      }
    }

    EndDialog(IDOK);
  } else {
    // No file or directory here.
    CString str;
    str.FormatMessage("File %1!s! doesn't exist.", (const char *)file_path);
    AfxMessageBox(str, MB_OK);
  }
}

void FileSystemOpenDlg::SetInitialDir(const char *pDir, const char *pPathID) {
  current_dir_ = pDir;
  path_id_ = pPathID ? pPathID : "";
}

CString FileSystemOpenDlg::GetFilename() const { return file_name_; }

BOOL FileSystemOpenDlg::OnInitDialog() {
  CDialog::OnInitDialog();

  // Setup our anchor list.
  AddAnchor(IDC_FILE_LIST, 2, 2);
  AddAnchor(IDC_FILE_LIST, 3, 3);

  AddAnchor(IDC_FILENAME, 1, 3);
  AddAnchor(IDC_FILENAME, 3, 3);
  AddAnchor(IDC_FILENAME, 2, 2);

  AddAnchor(IDC_FILENAME_LABEL, 0, 0);
  AddAnchor(IDC_FILENAME_LABEL, 2, 0);
  AddAnchor(IDC_FILENAME_LABEL, 1, 3);
  AddAnchor(IDC_FILENAME_LABEL, 3, 3);

  AddAnchor(IDOK, 0, 2);
  AddAnchor(IDOK, 2, 2);
  AddAnchor(IDOK, 1, 3);
  AddAnchor(IDOK, 3, 3);

  AddAnchor(IDCANCEL, 0, 2);
  AddAnchor(IDCANCEL, 2, 2);
  AddAnchor(IDCANCEL, 1, 3);
  AddAnchor(IDCANCEL, 3, 3);

  AddAnchor(IDC_LOOKIN, 2, 2);

  AddAnchor(IDC_UP_BUTTON, 0, 2);
  AddAnchor(IDC_UP_BUTTON, 2, 2);

  // Setup our image list.
  images_list_.Create(PREVIEW_IMAGE_SIZE, PREVIEW_IMAGE_SIZE, ILC_COLOR32, 0,
                      512);

  bitmap_folder_.LoadBitmap(IDB_LABEL_FOLDER);
  label_folder_ = images_list_.Add(&bitmap_folder_, (CBitmap *)nullptr);

  bitmap_mdl_.LoadBitmap(IDB_LABEL_MDL);
  label_mdl_ = images_list_.Add(&bitmap_mdl_, (CBitmap *)nullptr);

  bitmap_file_.LoadBitmap(IDB_LABEL_FILE);
  label_file_ = images_list_.Add(&bitmap_file_, (CBitmap *)nullptr);

  m_FileList.SetImageList(&images_list_, LVSIL_NORMAL);

  // Populate the list with the contents of our current directory.
  PopulateListControl();

  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
  return TRUE;
}

void FileSystemOpenDlg::GetEntries(const char *pMask,
                                   CUtlVector<CString> &entries,
                                   GetEntriesMode_t mode) {
  CString searchStr = current_dir_ + "\\" + pMask;

  // Workaround Steam bug.
  if (searchStr == ".\\*.*") searchStr = "*.*";

  FileFindHandle_t handle;
  const char *file_name = file_system_->FindFirst(searchStr, &handle);

  while (file_name) {
    const bool is_dir = file_system_->FindIsDirectory(handle);
    if ((mode == GETENTRIES_DIRECTORIES_ONLY && is_dir) ||
        (mode == GETENTRIES_FILES_ONLY && !is_dir)) {
      entries.AddToTail(file_name);
    }

    file_name = file_system_->FindNext(handle);
  }

  file_system_->FindClose(handle);
}

class SteamJpegSourceMgr : public jpeg_source_mgr {
 public:
  SteamJpegSourceMgr(IFileSystem *pFileSystem, FileHandle_t fp) {
    this->init_source = &SteamJpegSourceMgr::imp_init_source;
    this->fill_input_buffer = &SteamJpegSourceMgr::imp_fill_input_buffer;
    this->skip_input_data = &SteamJpegSourceMgr::imp_skip_input_data;
    this->resync_to_restart = &SteamJpegSourceMgr::imp_resync_to_restart;
    this->term_source = &SteamJpegSourceMgr::imp_term_source;

    this->next_input_byte = 0;
    this->bytes_in_buffer = 0;

    the_data.SetSize(pFileSystem->Size(fp));
    is_success = pFileSystem->Read(the_data.Base(), the_data.Count(), fp) ==
                 the_data.Count();
  }

  bool Init(IFileSystem *pFileSystem, FileHandle_t fp) {
    the_data.SetSize(pFileSystem->Size(fp));
    return pFileSystem->Read(the_data.Base(), the_data.Count(), fp) ==
           the_data.Count();
  }

  static void imp_init_source(j_decompress_ptr cinfo) {}

  static boolean imp_fill_input_buffer(j_decompress_ptr cinfo) {
    Assert(false);  // They should never need to call these functions since we
                    // give them all the data up front.
    return 0;
  }

  static void imp_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
    Assert(false);  // They should never need to call these functions since we
                    // give them all the data up front.
  }

  static boolean imp_resync_to_restart(j_decompress_ptr cinfo, int desired) {
    Assert(false);  // They should never need to call these functions since we
                    // give them all the data up front.
    return false;
  }

  static void imp_term_source(j_decompress_ptr cinfo) {}

 public:
  CUtlVector<char> the_data;
  bool is_success;
};

bool ReadJpeg(IFileSystem *file_system, const char *file_name,
              CUtlVector<unsigned char> &buffer, int &width, int &height,
              const char *path_id) {
  // Read the data.
  FileHandle_t fp = file_system->Open(file_name, "rb", path_id);
  if (fp == FILESYSTEM_INVALID_HANDLE) return false;

  SteamJpegSourceMgr jpeg_source_mgr{file_system, fp};
  file_system->Close(fp);
  if (!jpeg_source_mgr.is_success) return false;

  jpeg_source_mgr.bytes_in_buffer = jpeg_source_mgr.the_data.Count();
  jpeg_source_mgr.next_input_byte =
      (unsigned char *)jpeg_source_mgr.the_data.Base();

  // Load the jpeg.
  jpeg_decompress_struct jpegInfo;
  jpeg_error_mgr jerr;

  memset(&jpegInfo, 0, sizeof(jpegInfo));
  jpegInfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&jpegInfo);
  jpegInfo.src = &jpeg_source_mgr;

  if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK) {
    return false;
  }

  // start the decompress with the jpeg engine.
  if (jpeg_start_decompress(&jpegInfo) != TRUE ||
      jpegInfo.output_components != 3) {
    jpeg_destroy_decompress(&jpegInfo);
    return false;
  }

  // now that we've started the decompress with the jpeg lib, we have the
  // attributes of the image ready to be read out of the decompress struct.
  int row_stride = jpegInfo.output_width * jpegInfo.output_components;
  int mem_required =
      jpegInfo.image_height * jpegInfo.image_width * jpegInfo.output_components;
  JSAMPROW row_pointer[1];
  int cur_row = 0;

  width = jpegInfo.output_width;
  height = jpegInfo.output_height;

  // allocate the memory to read the image data into.
  buffer.SetSize(mem_required);

  // read in all the scan lines of the image into our image data buffer.
  bool working = true;
  while (working && (jpegInfo.output_scanline < jpegInfo.output_height)) {
    row_pointer[0] = &(buffer[cur_row * row_stride]);
    if (jpeg_read_scanlines(&jpegInfo, row_pointer, 1) != TRUE) {
      working = false;
    }
    ++cur_row;
  }

  if (!working) {
    jpeg_destroy_decompress(&jpegInfo);
    return false;
  }

  jpeg_finish_decompress(&jpegInfo);
  return true;
}

void DownsampleRGBToRGBAImage(CUtlVector<unsigned char> &srcData, int srcWidth,
                              int srcHeight,
                              CUtlVector<unsigned char> &destData,
                              int destWidth, int destHeight) {
  int srcPixelSize = 3;
  int destPixelSize = 4;
  destData.SetSize(destWidth * destHeight * destPixelSize);
  memset(destData.Base(), 0xFF, destWidth * destHeight * destPixelSize);

  // This preserves the aspect ratio of the image.
  int scaledDestWidth = destWidth;
  int scaledDestHeight = destHeight;
  int destOffsetX = 0, destOffsetY = 0;
  if (srcWidth > srcHeight) {
    scaledDestHeight = (srcHeight * destHeight) / srcWidth;
    destOffsetY = (destHeight - scaledDestHeight) / 2;
  } else if (srcHeight > srcWidth) {
    scaledDestWidth = (srcWidth * destWidth) / srcHeight;
    destOffsetX = (destWidth - scaledDestWidth) / 2;
  }

  for (int destY = 0; destY < scaledDestHeight; destY++) {
    unsigned char *pDestLine =
        &destData[(destY + destOffsetY) * destWidth * destPixelSize +
                  (destOffsetX * destPixelSize)];
    unsigned char *pDestPos = pDestLine;

    float destYPercent = (float)destY / (scaledDestHeight - 1);
    int srcY = (int)(destYPercent * (srcHeight - 1));

    for (int destX = 0; destX < scaledDestWidth; destX++) {
      float destXPercent = (float)destX / (scaledDestWidth - 1);

      int srcX = (int)(destXPercent * (srcWidth - 1));
      unsigned char *pSrcPos =
          &srcData[(srcY * srcWidth + srcX) * srcPixelSize];
      pDestPos[0] = pSrcPos[2];
      pDestPos[1] = pSrcPos[1];
      pDestPos[2] = pSrcPos[0];
      pDestPos[3] = 255;

      pDestPos += destPixelSize;
    }
  }
}

CBitmap *SetupJpegLabel(IFileSystem *pFileSystem, CString filename,
                        int labelSize, const char *pPathID) {
  CBitmap *pBitmap = g_BitmapCache.Find(filename);
  if (pBitmap) return pBitmap;

  CUtlVector<unsigned char> data;
  int width, height;
  if (!ReadJpeg(pFileSystem, filename, data, width, height, pPathID))
    return nullptr;

  CUtlVector<unsigned char> downsampled;
  DownsampleRGBToRGBAImage(data, width, height, downsampled, labelSize,
                           labelSize);

  pBitmap = new CBitmap;
  if (pBitmap->CreateBitmap(labelSize, labelSize, 1, 32, downsampled.Base())) {
    g_BitmapCache.AddToCache(pBitmap, filename, downsampled.Count(), true);
    return pBitmap;
  } else {
    delete pBitmap;
    return nullptr;
  }
}

int FileSystemOpenDlg::SetupLabelImage(FileInfo *pInfo, CString name,
                                       bool bIsDir) {
  if (bIsDir) return label_folder_;

  CString extension = name.Right(4);
  extension.MakeLower();
  if (extension == ".jpg" || extension == ".jpeg") {
    pInfo->bitmap = SetupJpegLabel(file_system_, current_dir_ + "\\" + name,
                                   PREVIEW_IMAGE_SIZE, GetPathID());
    if (pInfo->bitmap)
      return images_list_.Add(pInfo->bitmap, (CBitmap *)nullptr);
    else
      return label_file_;
  } else {
    return (extension == ".mdl") ? label_mdl_ : label_file_;
  }
}

void FilterMdlAndJpgFiles(CUtlVector<CString> &files) {
  // Build a dictionary with all the .jpeg files.
  CUtlDict<int, int> jpgFiles;
  for (int i = 0; i < files.Count(); i++) {
    CString extension = files[i].Right(4);
    extension.MakeLower();
    if (extension == ".jpg" || extension == ".jpeg") {
      CString base = files[i].Left(files[i].GetLength() - 4);
      jpgFiles.Insert(base, 1);
    }
  }

  // Now look for all mdls and remove them if they have a jpg.
  for (int i = 0; i < files.Count(); i++) {
    CString extension = files[i].Right(4);
    extension.MakeLower();
    if (extension == ".mdl") {
      CString base = files[i].Left(files[i].GetLength() - 4);
      if (jpgFiles.Find(base) != -1) {
        files.Remove(i);
        --i;
      }
    }
  }
}

int CALLBACK FileListSortCallback(LPARAM lParam1, LPARAM lParam2,
                                  LPARAM lParamSort) {
  FileSystemOpenDlg *pDlg = (FileSystemOpenDlg *)lParamSort;
  FileInfo *pInfo1 = &pDlg->m_FileInfos[lParam1];
  FileInfo *pInfo2 = &pDlg->m_FileInfos[lParam2];
  if (pInfo1->isDirectory != pInfo2->isDirectory)
    return pInfo1->isDirectory ? -1 : 1;

  return Q_stricmp(pInfo1->fileName, pInfo2->fileName);
}

void RemoveDuplicates(CUtlVector<CString> &files) {
  CUtlDict<int, int> uniqueFilenames;
  for (int i = 0; i < files.Count(); i++) {
    int iPreviousIndex = uniqueFilenames.Find(files[i]);
    if (iPreviousIndex == -1) {
      uniqueFilenames.Insert(files[i], i);
    } else {
      files.Remove(i);
      --i;
    }
  }
}

void FileSystemOpenDlg::PopulateListControl() {
  m_FileList.DeleteAllItems();
  g_BitmapCache.UnlockAll();
  m_LookInLabel.SetWindowText(CString("[ROOT]\\") + current_dir_);

  int iItem = 0;

  // First add directories at the top.
  CUtlVector<CString> directories;
  GetEntries("*.*", directories, GETENTRIES_DIRECTORIES_ONLY);
  RemoveDuplicates(directories);

  for (int i = 0; i < directories.Count(); i++) {
    if (directories[i] == "." || directories[i] == "..") continue;

    LVITEM item;
    item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    item.iItem = iItem++;
    item.iSubItem = 0;
    item.pszText = directories[i].GetBuffer(0);

    item.lParam = m_FileInfos.AddToTail();
    m_FileInfos[item.lParam].isDirectory = true;
    m_FileInfos[item.lParam].fileName = directories[i];
    item.iImage =
        SetupLabelImage(&m_FileInfos[item.lParam], directories[i], true);

    m_FileList.InsertItem(&item);
  }

  CUtlVector<CString> files;
  for (int iMask = 0; iMask < file_name_masks_.Count(); iMask++) {
    GetEntries(file_name_masks_[iMask], files, GETENTRIES_FILES_ONLY);
  }

  RemoveDuplicates(files);
  if (enable_mdl_jpg_filter_) FilterMdlAndJpgFiles(files);

  for (int i = 0; i < files.Count(); i++) {
    LVITEM item;
    item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    item.iItem = iItem++;
    item.iSubItem = 0;
    item.iImage = label_mdl_;
    item.pszText = files[i].GetBuffer(0);

    item.lParam = m_FileInfos.AddToTail();
    m_FileInfos[item.lParam].isDirectory = false;
    m_FileInfos[item.lParam].fileName = files[i];
    item.iImage = SetupLabelImage(&m_FileInfos[item.lParam], files[i], false);

    m_FileList.InsertItem(&item);
  }

  m_FileList.SortItems(FileListSortCallback, (DWORD)this);
}

void FileSystemOpenDlg::AddFileMask(const char *pMask) {
  file_name_masks_.AddToTail(pMask);
}

BOOL FileSystemOpenDlg::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                               DWORD dwStyle, const RECT &rect,
                               CWnd *pParentWnd, UINT nID,
                               CCreateContext *pContext) {
  return CDialog::Create(IDD, pParentWnd);
}

int FileSystemOpenDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CDialog::OnCreate(lpCreateStruct) == -1) return -1;

  return 0;
}

LONG &GetSideCoord(RECT &rect, int iSide) {
  if (iSide == 0)
    return rect.left;
  else if (iSide == 1)
    return rect.top;
  else if (iSide == 2)
    return rect.right;
  else
    return rect.bottom;
}

LONG GetSideScreenCoord(CWnd *pWnd, int iSide) {
  RECT rect;
  pWnd->GetWindowRect(&rect);
  return GetSideCoord(rect, iSide);
}

void FileSystemOpenDlg::ProcessAnchor(WindowAnchor *pAnchor) {
  RECT rect, parentRect;
  GetWindowRect(&parentRect);
  pAnchor->window->GetWindowRect(&rect);

  GetSideCoord(rect, pAnchor->side) =
      GetSideCoord(parentRect, pAnchor->parentSide) + pAnchor->originalDist;

  ScreenToClient(&rect);
  pAnchor->window->MoveWindow(&rect);
}

void FileSystemOpenDlg::AddAnchor(int iDlgItem, int iSide, int iParentSide) {
  CWnd *pItem = GetDlgItem(iDlgItem);
  if (!pItem) return;

  WindowAnchor *pAnchor = &window_anchors_[window_anchors_.AddToTail()];
  pAnchor->window = pItem;
  pAnchor->side = iSide;
  pAnchor->parentSide = iParentSide;
  pAnchor->originalDist =
      GetSideScreenCoord(pItem, iSide) - GetSideScreenCoord(this, iParentSide);
}

void FileSystemOpenDlg::OnSize(UINT nType, int cx, int cy) {
  CDialog::OnSize(nType, cx, cy);

  for (int i = 0; i < window_anchors_.Count(); i++)
    ProcessAnchor(&window_anchors_[i]);

  if (m_FileList.GetSafeHwnd()) PopulateListControl();
}

void FileSystemOpenDlg::OnDblclkFileList(NMHDR *pNMHDR, LRESULT *pResult) {
  /*int iSelected = m_FileList.GetNextItem( -1, LVNI_SELECTED );
  if ( iSelected != -1 )
  {
          DWORD iItem = m_FileList.GetItemData( iSelected );
          if ( iItem < (DWORD)m_FileInfos.Count() )
          {
                  CFileInfo *pInfo = &m_FileInfos[iItem];
                  if ( pInfo->m_bIsDir )
                  {
                          m_CurrentDir += "\\" + m_FileInfos[iItem].m_Name;
                          PopulateListControl();
                  }
                  else
                  {
                          m_Filename = m_CurrentDir + "\\" +
  m_FileInfos[iItem].m_Name; EndDialog( IDOK );
                  }
          }
          else
          {
                  Assert( false );
          }
  }*/
  OnOK();

  *pResult = 0;
}

void FileSystemOpenDlg::OnUpButton() {
  char str[SOURCE_MAX_PATH];
  Q_strncpy(str, current_dir_, sizeof(str));
  Q_StripLastDir(str, sizeof(str));

  if (str[0] == 0) Q_strncpy(str, ".", sizeof(str));

  if (str[strlen(str) - 1] == '\\' || str[strlen(str) - 1] == '/')
    str[strlen(str) - 1] = 0;

  current_dir_ = str;
  PopulateListControl();
}

void FileSystemOpenDlg::OnItemchangedFileList(NMHDR *pNMHDR, LRESULT *pResult) {
  NM_LISTVIEW *pNMListView = (NM_LISTVIEW *)pNMHDR;

  DWORD iItem = m_FileList.GetItemData(pNMListView->iItem);
  if (iItem < (DWORD)m_FileInfos.Count()) {
    FileInfo *pInfo = &m_FileInfos[iItem];
    if ((pNMListView->uChanged & LVIF_STATE) &&
        (pNMListView->uNewState & LVIS_SELECTED)) {
      m_FilenameControl.SetWindowText(pInfo->fileName);
    }
  }

  *pResult = 0;
}

void FileSystemOpenDlg::SetFilterMdlAndJpgFiles(bool bFilter) {
  enable_mdl_jpg_filter_ = bFilter;
}

const char *FileSystemOpenDlg::GetPathID() {
  return path_id_ == "" ? nullptr : (const char *)path_id_;
}

// IFileSystemOpenDialog implementation.
class FileSystemOpenDialog : public IFileSystemOpenDialog {
 public:
  FileSystemOpenDialog()
      : file_system_open_dlg_{nullptr},
        parent_window_{nullptr},
        is_last_modal_windows_dialog_{false} {
    relative_file_path_[0] = '\0';
  }

  void Release() override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    delete file_system_open_dlg_;
    delete this;
  }

  // You must call this first to set the hwnd.
  void Init(CreateInterfaceFn factory, void *parentHwnd) override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(!file_system_open_dlg_);

    parent_window_ = reinterpret_cast<HWND>(parentHwnd);
    file_system_open_dlg_ =
        new FileSystemOpenDlg(factory, CWnd::FromHandle(parent_window_));
  }

  void AddFileMask(const char *pMask) override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(file_system_open_dlg_);

    file_system_open_dlg_->AddFileMask(pMask);
  }

  void SetInitialDir(const char *pDir, const char *pPathID) override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(file_system_open_dlg_);

    file_system_open_dlg_->SetInitialDir(pDir, pPathID);
  }

  void SetFilterMdlAndJpgFiles(bool bFilter) override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(file_system_open_dlg_);

    file_system_open_dlg_->SetFilterMdlAndJpgFiles(bFilter);
  }

  void GetFilename(char *pOut, int outLen) const override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(file_system_open_dlg_);

    strcpy_s(pOut, outLen,
             is_last_modal_windows_dialog_
                 ? relative_file_path_
                 : file_system_open_dlg_->GetFilename().operator LPCSTR());
  }

  bool DoModal() override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(file_system_open_dlg_);

    is_last_modal_windows_dialog_ = false;
    return file_system_open_dlg_->DoModal() == IDOK;
  }

  bool DoModal_WindowsDialog() override {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    Assert(file_system_open_dlg_);

    is_last_modal_windows_dialog_ = true;

    // Get the full filename, then make sure it's a relative path.
    char file_path[SOURCE_MAX_PATH] = {0};
    if (file_system_open_dlg_->file_name_masks_.Count() > 0) {
      CString extension =
          file_system_open_dlg_
              ->file_name_masks_
                  [file_system_open_dlg_->file_name_masks_.Count() - 1]
              .Right(4);
      const char *extension_ptr = extension;

      if (extension_ptr[0] == '.') strcpy_s(file_path, extension_ptr + 1);
    }

    const char *full_file_path =
        file_system_open_dlg_->file_system_->RelativePathToFullPath(
            file_system_open_dlg_->current_dir_,
            file_system_open_dlg_->path_id_, file_path,
            SOURCE_ARRAYSIZE(file_path));
    strcat_s(file_path, "\\");

    // Build the list of file filters.
    char filters[1024];
    if (file_system_open_dlg_->file_name_masks_.Count() == 0) {
      strcpy_s(filters, "All Files (*.*)|*.*||");
    } else {
      filters[0] = '\0';

      for (int i = 0; i < file_system_open_dlg_->file_name_masks_.Count();
           i++) {
        if (i > 0) strcat_s(filters, "|");

        strcat_s(filters, file_system_open_dlg_->file_name_masks_[i]);
        strcat_s(filters, "|");
        strcat_s(filters, file_system_open_dlg_->file_name_masks_[i]);

        if (full_file_path) {
          strcat_s(file_path, file_system_open_dlg_->file_name_masks_[i]);
          strcat_s(file_path, ";");
        }
      }

      Q_strncat(filters, "||", sizeof(filters), COPY_ALL_CHARACTERS);
    }

    CFileDialog file_dialog{
        true,                                     // open dialog?
        file_path[0] == 0 ? nullptr : file_path,  // default file extension
        full_file_path,                           // initial filename
        OFN_ENABLESIZING,                         // flags
        filters,
        CWnd::FromHandle(parent_window_)};

    while (file_dialog.DoModal() == IDOK) {
      // Make sure we can make this into a relative path.
      if (file_system_open_dlg_->file_system_->FullPathToRelativePath(
              file_dialog.GetPathName(), relative_file_path_,
              SOURCE_ARRAYSIZE(relative_file_path_))) {
        // Replace .jpg or .jpeg extension with .mdl?
        char *end = relative_file_path_;

        while (Q_stristr(end + 1, ".jpeg") || Q_stristr(end + 1, ".jpg"))
          end = std::max(Q_stristr(end, ".jpeg"), Q_stristr(end, ".jpg"));

        if (end && end != relative_file_path_) {
          Q_strncpy(end, ".mdl",
                    sizeof(relative_file_path_) - (end - relative_file_path_));
        }

        return true;
      }

      AfxMessageBox(IDS_NO_RELATIVE_PATH);
    }

    return false;
  }

 private:
  FileSystemOpenDlg *file_system_open_dlg_;
  HWND parent_window_;

  char relative_file_path_[SOURCE_MAX_PATH];
  bool is_last_modal_windows_dialog_;
};

EXPOSE_INTERFACE(FileSystemOpenDialog, IFileSystemOpenDialog,
                 FILESYSTEMOPENDIALOG_VERSION);
