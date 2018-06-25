// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "basefilesystem.h"

#ifdef OS_POSIX
#include "linux_support.cpp"
#endif

#ifdef OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

#include "tier0/include/dbg.h"
#include "tier0/include/threadtools.h"
#ifdef OS_WIN
#include "tier0/include/tslist.h"
#endif
#include "tier0/include/vcrmode.h"
#include "tier0/include/vprof.h"
#include "tier1/convar.h"
#include "tier1/fmtstr.h"
#include "tier1/utlrbtree.h"

#include "tier0/include/memdbgon.h"

static_assert(SEEK_CUR == FILESYSTEM_SEEK_CURRENT);
static_assert(SEEK_SET == FILESYSTEM_SEEK_HEAD);
static_assert(SEEK_END == FILESYSTEM_SEEK_TAIL);

class CFileSystem_Stdio : public CBaseFileSystem {
 public:
  CFileSystem_Stdio() : is_mounted_{false}, can_be_async_{true} {}
  ~CFileSystem_Stdio() { Assert(!is_mounted_); }

  // Used to get at older versions
  void *QueryInterface(const char *pInterfaceName);

  // Higher level filesystem methods requiring specific behavior
  virtual void GetLocalCopy(const char *pFileName);
  virtual int HintResourceNeed(const char *hintlist, int forgetEverything);
  virtual bool IsFileImmediatelyAvailable(const char *pFileName);
  virtual WaitForResourcesHandle_t WaitForResources(const char *resourcelist);
  virtual bool GetWaitForResourcesProgress(WaitForResourcesHandle_t handle,
                                           float *progress /* out */,
                                           bool *complete /* out */);
  virtual void CancelWaitForResources(WaitForResourcesHandle_t handle);
  virtual bool IsSteam() const { return false; }
  virtual FilesystemMountRetval_t MountSteamContent(int nExtraAppId = -1) {
    return FILESYSTEM_MOUNT_OK;
  }

  bool GetOptimalIOConstraints(FileHandle_t hFile, unsigned *pOffsetAlign,
                               unsigned *pSizeAlign, unsigned *pBufferAlign);
  void *AllocOptimalReadBuffer(FileHandle_t hFile, unsigned nSize,
                               unsigned nOffset);
  void FreeOptimalReadBuffer(void *p);

 protected:
  // implementation of CBaseFileSystem virtual functions
  FILE *FS_fopen(const char *filename, const char *options, unsigned flags,
                 int64_t *size, CFileLoadInfo *pInfo) override;
  void FS_setbufsize(FILE *fp, size_t nBytes) override;
  void FS_fclose(FILE *fp) override;
  void FS_fseek(FILE *fp, int64_t pos, int seekType) override;
  int64_t FS_ftell(FILE *fp) override;
  int FS_feof(FILE *fp) override;
  size_t FS_fread(void *dest, size_t destSize, size_t size, FILE *fp) override;
  size_t FS_fwrite(const void *src, size_t size, FILE *fp) override;
  bool FS_setmode(FILE *fp, FileMode_t mode) override;
  size_t FS_vfprintf(FILE *fp, const char *fmt, va_list list) override;
  int FS_ferror(FILE *fp) override;
  int FS_fflush(FILE *fp) override;
  char *FS_fgets(char *dest, int destSize, FILE *fp) override;
  int FS_stat(const char *path, struct _stat *buf) override;
  int FS_fexists(const char *path) override;
  int FS_chmod(const char *path, int pmode) override;
  HANDLE FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat) override;
  bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat) override;
  bool FS_FindClose(HANDLE handle) override;
  int FS_GetSectorSize(FILE *) override;

 private:
  bool CanAsync() const { return can_be_async_; }

  const bool is_mounted_;
  const bool can_be_async_;
};

// Per-file worker classes.

the_interface IStdFilesystemFile {
 public:
  virtual ~IStdFilesystemFile() {}
  virtual int FS_setbufsize(size_t nBytes) = 0;
  virtual int FS_fclose() = 0;
  virtual int FS_fseek(int64_t pos, int seekType) = 0;
  virtual int64_t FS_ftell() = 0;
  virtual int FS_feof() = 0;
  virtual size_t FS_fread(void *dest, size_t destSize, size_t size) = 0;
  virtual size_t FS_fwrite(const void *src, size_t size) = 0;
  virtual bool FS_setmode(FileMode_t mode) = 0;
  virtual size_t FS_vfprintf(const char *fmt, va_list list) = 0;
  virtual int FS_ferror() = 0;
  virtual int FS_fflush() = 0;
  virtual char *FS_fgets(char *dest, int destSize) = 0;
  virtual unsigned long FS_GetSectorSize() const { return 1; }
};

class StdioFile : public IStdFilesystemFile {
 public:
  static StdioFile *FS_fopen(const char *filename, const char *options,
                             int64_t *size) {
    // Stop newline characters at end of filename
    Assert(!strchr(filename, '\n') && !strchr(filename, '\r'));

    FILE *fd;
    if (!fopen_s(&fd, filename, options) && size) {
      struct _stati64 buf;
      int rt = _stati64(filename, &buf);
      if (rt == 0) *size = buf.st_size;
    }

#ifndef OS_WIN
    // try opening the lower cased version
    if (!fd && !strchr(options, 'w') && !strchr(options, '+')) {
      const char *file = findFileInDirCaseInsensitive(filename);
      if (file) {
        fd = fopen(file, options);

        if (fd && size) {
          struct _stat buf;
          int rt = _stat(file, &buf);
          if (rt == 0) {
            *size = buf.st_size;
          }
        }
      }
    }
#endif

    return fd ? new StdioFile(fd) : nullptr;
  }

  int FS_setbufsize(size_t nBytes) override {
#ifdef OS_WIN
    return setvbuf(file_, nullptr, nBytes ? _IOFBF : _IONBF, nBytes);
#endif
  }

  int FS_fclose() override { return fclose(file_); }

  int FS_fseek(int64_t pos, int seekType) override {
    return _fseeki64(file_, pos, seekType);
  }

  int64_t FS_ftell() override { return _ftelli64(file_); }

  int FS_feof() override { return feof(file_); }

  size_t FS_fread(void *dest, size_t destSize, size_t size) override {
    // read (size) of bytes to ensure truncated reads returns bytes read and
    // not 0
    return fread_s(dest, destSize, 1, size, file_);
  }

  // This routine breaks data into chunks if the amount to be written is
  // beyond WRITE_CHUNK (512kb) Windows can fail on monolithic writes of ~12MB
  // or more, so we work around that here
  size_t FS_fwrite(const void *src, size_t size) override {
    static constexpr size_t kWriteChunkBytes{512 * 1024};

    if (size > kWriteChunkBytes) {
      size_t remaining = size;
      const u8 *current = (const u8 *)src;
      size_t total = 0;

      while (remaining > 0) {
        size_t bytesToCopy = std::min(remaining, kWriteChunkBytes);

        total += fwrite(current, 1, bytesToCopy, file_);

        remaining -= bytesToCopy;
        current += bytesToCopy;
      }

      Assert(total == size);
      return total;
    }

    // return number of bytes written (because we have size = 1, count = bytes,
    // so it return bytes)
    return fwrite(src, 1, size, file_);
  }

  bool FS_setmode(FileMode_t mode) override {
#ifdef OS_WIN
    int fd = _fileno(file_);
    int newMode = (mode == FM_BINARY) ? _O_BINARY : _O_TEXT;
    return _setmode(fd, newMode) != -1;
#else
    return false;
#endif
  }

  size_t FS_vfprintf(const char *fmt, va_list list) override {
    return vfprintf_s(file_, fmt, list);
  }

  int FS_ferror() override { return ferror(file_); }

  int FS_fflush() override { return fflush(file_); }

  char *FS_fgets(char *dest, int destSize) override {
    return fgets(dest, destSize, file_);
  }

 private:
  StdioFile(FILE *file) : file_{file} {}

  FILE *file_;
};

#ifndef _RETAIL
static bool UseOptimalBufferAllocation() {
  static bool should_use_optimal_buffer{
      !IsLinux() &&
      Q_stristr(Plat_GetCommandLine(), "-unbuffered_io") != nullptr};
  return should_use_optimal_buffer;
}
ConVar filesystem_unbuffered_io{"filesystem_unbuffered_io", "1", 0, ""};
static inline bool UseUnbufferedIO() {
  return UseOptimalBufferAllocation() && filesystem_unbuffered_io.GetBool();
}
#else
static inline bool UseUnbufferedIO() { return true }
#endif

ConVar filesystem_native{"filesystem_native", "1", 0, "Use native FS or STDIO"};
ConVar filesystem_max_stdio_read{"filesystem_max_stdio_read", "64", 0, ""};
ConVar filesystem_report_buffered_io{"filesystem_report_buffered_io", "0"};

static HANDLE OpenWin32File(const char *file_path, bool is_overlapped,
                            bool is_unbuffered, int64_t *file_size_bytes) {
  DWORD create_flags{FILE_ATTRIBUTE_NORMAL};
  if (is_overlapped) create_flags |= FILE_FLAG_OVERLAPPED;
  if (is_unbuffered) create_flags |= FILE_FLAG_NO_BUFFERING;

  HANDLE file{::CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, create_flags, nullptr)};
  if (file != INVALID_HANDLE_VALUE && !*file_size_bytes) {
    LARGE_INTEGER file_size;

    if (GetFileSizeEx(file, &file_size)) {
      *file_size_bytes = file_size.QuadPart;
    } else {
      CloseHandle(file);
      file = INVALID_HANDLE_VALUE;
    }
  }

  return file;
}

#ifdef OS_WIN

#ifdef OS_WIN
unsigned long GetSectorSize(const char *pszFilename) {
  if ((!pszFilename[0] || !pszFilename[1]) ||
      (pszFilename[0] == '\\' && pszFilename[1] == '\\') ||
      (pszFilename[0] == '/' && pszFilename[1] == '/')) {
    // Cannot determine sector size with a UNC path (need volume identifier)
    return 0;
  }

#if defined(OS_WIN) && !defined(FILESYSTEM_STEAM)
  char szAbsoluteFilename[MAX_FILEPATH];
  if (pszFilename[1] != ':') {
    Q_MakeAbsolutePath(szAbsoluteFilename, sizeof(szAbsoluteFilename),
                       pszFilename);
    pszFilename = szAbsoluteFilename;
  }

  DWORD sectorSize = 1;

  struct DriveSectorSize_t {
    char volume;
    DWORD sectorSize;
  };

  static DriveSectorSize_t cachedSizes[4];

  char volume = tolower(*pszFilename);

  usize i;
  for (i = 0; i < std::size(cachedSizes) && cachedSizes[i].volume; i++) {
    if (cachedSizes[i].volume == volume) {
      sectorSize = cachedSizes[i].sectorSize;
      break;
    }
  }

  if (sectorSize == 1) {
    char root[4] = "X:\\";
    root[0] = *pszFilename;

    DWORD ignored;
    if (!GetDiskFreeSpace(root, &ignored, &sectorSize, &ignored, &ignored)) {
      sectorSize = 0;
    }

    if (i < std::size(cachedSizes)) {
      cachedSizes[i].volume = volume;
      cachedSizes[i].sectorSize = sectorSize;
    }
  }

  return sectorSize;
#else
  return 0;
#endif
}

ConVar filesystem_use_overlapped_io{"filesystem_use_overlapped_io", "1", 0,
                                    "Enable windows overlapped (async) io"};
static inline bool UseOverlappedIO() {
  return filesystem_use_overlapped_io.GetBool();
};

class Win32ReadOnlyFile : public IStdFilesystemFile {
 public:
  static bool CanOpen(const char *file_path, const char *options) {
    return (options[0] == 'r' && options[1] == 'b' && options[2] == 0 &&
            filesystem_native.GetBool());
  }

  static Win32ReadOnlyFile *FS_fopen(const char *file_path, const char *options,
                                     int64_t *size) {
    Assert(CanOpen(file_path, options));

    unsigned long storage_sector_size{0};
    const bool should_try_unbuffered = {
        UseUnbufferedIO() &&
        (storage_sector_size = GetSectorSize(file_path)) != 0};
    const bool is_overlapped{UseOverlappedIO()};

    HANDLE unbuffered_file{INVALID_HANDLE_VALUE};
    int64_t file_size{0};

    if (should_try_unbuffered) {
      unbuffered_file =
          OpenWin32File(file_path, is_overlapped, true, &file_size);
      if (unbuffered_file == INVALID_HANDLE_VALUE) {
        return nullptr;
      }
    }

    HANDLE buffered_file{
        OpenWin32File(file_path, is_overlapped, false, &file_size)};
    if (buffered_file == INVALID_HANDLE_VALUE) {
      if (unbuffered_file != INVALID_HANDLE_VALUE) {
        CloseHandle(unbuffered_file);
      }

      return nullptr;
    }

    if (size) *size = file_size;

    return new Win32ReadOnlyFile{unbuffered_file, buffered_file,
                                 storage_sector_size ? storage_sector_size : 1,
                                 file_size, is_overlapped};
  }

  int FS_setbufsize(size_t nBytes) override { return 0; }

  int FS_fclose() override {
    if (unbuffered_file_ != INVALID_HANDLE_VALUE) {
      CloseHandle(unbuffered_file_);
    }

    if (buffered_file_ != INVALID_HANDLE_VALUE) {
      CloseHandle(buffered_file_);
    }

    return 0;
  }

  int FS_fseek(int64_t pos, int seekType) override {
    switch (seekType) {
      case SEEK_SET:
        read_offset_ = pos;
        break;

      case SEEK_CUR:
        read_offset_ += pos;
        break;

      case SEEK_END:
        read_offset_ = file_size_ - pos;
        break;
    }

    return 0;
  }

  int64_t FS_ftell() override { return read_offset_; }

  int FS_feof() override { return read_offset_ >= file_size_; }

  size_t FS_fread(void *dest, size_t destSize, size_t size) override;

  size_t FS_fwrite(const void *src, size_t size) override { return 0; }

  bool FS_setmode(FileMode_t mode) override {
    Error("Can't set mode, open a second file in right mode\n");
    return false;
  }

  size_t FS_vfprintf(const char *fmt, va_list list) override { return 0; }

  int FS_ferror() override { return 0; }

  int FS_fflush() override { return 0; }

  char *FS_fgets(char *dest, int destSize) override {
    if (FS_feof()) return nullptr;

    int64_t nStartPos = read_offset_;
    size_t nBytesRead = FS_fread(dest, destSize, destSize);
    if (!nBytesRead) return nullptr;

    dest[std::min(nBytesRead, (size_t)(destSize - 1))] = '\0';
    char *pNewline = strchr(dest, '\n');

    if (pNewline) {
      // advance past, leave \n
      pNewline++;
      *pNewline = '\0';
    } else {
      pNewline = &dest[std::min(nBytesRead, (size_t)(destSize - 1))];
    }

    read_offset_ = nStartPos + (pNewline - dest) + 1;

    return dest;
  }

  unsigned long FS_GetSectorSize() const override { return sector_size_; }

 private:
  Win32ReadOnlyFile(HANDLE unbuffered_file, HANDLE buffered_file,
                    unsigned long sector_size, int64_t file_size,
                    bool is_overlapped)
      : unbuffered_file_{unbuffered_file},
        buffered_file_{buffered_file},
        read_offset_{0},
        file_size_{file_size},
        sector_size_{sector_size},
        is_overlapped_{is_overlapped} {}

  int64_t read_offset_;
  int64_t file_size_;
  HANDLE unbuffered_file_;
  HANDLE buffered_file_;
  CThreadFastMutex m_Mutex;
  unsigned long sector_size_;
  bool is_overlapped_;
};
#endif

// singleton

CFileSystem_Stdio g_FileSystem_Stdio;
#if defined(OS_WIN) && defined(DEDICATED)
CBaseFileSystem *BaseFileSystem_Stdio() { return &g_FileSystem_Stdio; }
#endif

// "hack" to allow us to not export a stdio version of the
// FILESYSTEM_INTERFACE_VERSION anywhere
#ifdef DEDICATED
IFileSystem *g_pFileSystem = &g_FileSystem_Stdio;
IBaseFileSystem *g_pBaseFileSystem = &g_FileSystem_Stdio;
#else
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CFileSystem_Stdio, IFileSystem,
                                  FILESYSTEM_INTERFACE_VERSION,
                                  g_FileSystem_Stdio);
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CFileSystem_Stdio, IBaseFileSystem,
                                  BASEFILESYSTEM_INTERFACE_VERSION,
                                  g_FileSystem_Stdio);
#endif

void *CFileSystem_Stdio::QueryInterface(const char *pInterfaceName) {
  // We also implement the IMatSystemSurface interface
  if (!strncmp(pInterfaceName, FILESYSTEM_INTERFACE_VERSION,
               strlen(FILESYSTEM_INTERFACE_VERSION) + 1))
    return implicit_cast<IFileSystem *>(this);

  return CBaseFileSystem::QueryInterface(pInterfaceName);
}

bool CFileSystem_Stdio::GetOptimalIOConstraints(FileHandle_t hFile,
                                                unsigned *pOffsetAlign,
                                                unsigned *pSizeAlign,
                                                unsigned *pBufferAlign) {
  unsigned sectorSize;

  if (hFile && UseOptimalBufferAllocation()) {
    CFileHandle *fh = (CFileHandle *)hFile;
    sectorSize = fh->GetSectorSize();

    if (!sectorSize ||
        (fh->m_pPackFileHandle &&
         (fh->m_pPackFileHandle->AbsoluteBaseOffset() % sectorSize))) {
      sectorSize = 1;
    }
  } else {
    sectorSize = 1;
  }

  if (pOffsetAlign) *pOffsetAlign = sectorSize;

  if (pSizeAlign) *pSizeAlign = sectorSize;

  if (pBufferAlign) *pBufferAlign = sectorSize;

  return sectorSize > 1;
}

void *CFileSystem_Stdio::AllocOptimalReadBuffer(FileHandle_t hFile,
                                                unsigned nSize,
                                                unsigned nOffset) {
  if (!UseOptimalBufferAllocation()) {
    return CBaseFileSystem::AllocOptimalReadBuffer(hFile, nSize, nOffset);
  }

  unsigned sectorSize;
  if (hFile != FILESYSTEM_INVALID_HANDLE) {
    CFileHandle *fh = (CFileHandle *)hFile;
    sectorSize = fh->GetSectorSize();

    if (!nSize) {
      nSize = fh->Size();
    }

    if (fh->m_pPackFileHandle) {
      nOffset += fh->m_pPackFileHandle->AbsoluteBaseOffset();
    }
  } else {
    // an invalid handle gets a fake "optimal" but valid buffer
    // this path is for a caller that isn't doing i/o,
    // but needs an "optimal" buffer that can end up passed to
    // FreeOptimalReadBuffer()
    sectorSize = 4;
  }

  bool bOffsetIsAligned = (nOffset % sectorSize == 0);
  unsigned nAllocSize =
      (bOffsetIsAligned) ? AlignValue(nSize, sectorSize) : nSize;

  unsigned nAllocAlignment = (bOffsetIsAligned) ? sectorSize : 4;
  return _aligned_malloc(nAllocSize, nAllocAlignment);
}

void CFileSystem_Stdio::FreeOptimalReadBuffer(void *p) {
  if (!UseOptimalBufferAllocation()) {
    CBaseFileSystem::FreeOptimalReadBuffer(p);
    return;
  }

  if (p) {
    _aligned_free(p);
  }
}

FILE *CFileSystem_Stdio::FS_fopen(const char *filename, const char *options,
                                  unsigned flags, int64_t *size,
                                  CFileLoadInfo *pInfo) {
  if (pInfo) pInfo->m_bLoadedFromSteamCache = false;

#ifdef OS_WIN
  if (Win32ReadOnlyFile::CanOpen(filename, options)) {
    IStdFilesystemFile *pFile =
        Win32ReadOnlyFile::FS_fopen(filename, options, size);
    if (pFile) return (FILE *)pFile;
  }
#endif

  return (FILE *)StdioFile::FS_fopen(filename, options, size);
}

void CFileSystem_Stdio::FS_setbufsize(FILE *fp, size_t nBytes) {
  ((IStdFilesystemFile *)fp)->FS_setbufsize(nBytes);
}

void CFileSystem_Stdio::FS_fclose(FILE *fp) {
  IStdFilesystemFile *pFile = ((IStdFilesystemFile *)fp);
  pFile->FS_fclose();
  delete pFile;
}

void CFileSystem_Stdio::FS_fseek(FILE *fp, int64_t pos, int seekType) {
  ((IStdFilesystemFile *)fp)->FS_fseek(pos, seekType);
}

int64_t CFileSystem_Stdio::FS_ftell(FILE *fp) {
  return ((IStdFilesystemFile *)fp)->FS_ftell();
}

int CFileSystem_Stdio::FS_feof(FILE *fp) {
  return ((IStdFilesystemFile *)fp)->FS_feof();
}

size_t CFileSystem_Stdio::FS_fread(void *dest, size_t destSize, size_t size,
                                   FILE *fp) {
  size_t nBytesRead =
      ((IStdFilesystemFile *)fp)->FS_fread(dest, destSize, size);
  Trace_FRead(nBytesRead, fp);

  return nBytesRead;
}

size_t CFileSystem_Stdio::FS_fwrite(const void *src, size_t size, FILE *fp) {
  size_t nBytesWritten = ((IStdFilesystemFile *)fp)->FS_fwrite(src, size);
  return nBytesWritten;
}

bool CFileSystem_Stdio::FS_setmode(FILE *fp, FileMode_t mode) {
  return ((IStdFilesystemFile *)fp)->FS_setmode(mode);
}

size_t CFileSystem_Stdio::FS_vfprintf(FILE *fp, const char *fmt, va_list list) {
  return ((IStdFilesystemFile *)fp)->FS_vfprintf(fmt, list);
}

int CFileSystem_Stdio::FS_ferror(FILE *fp) {
  return ((IStdFilesystemFile *)fp)->FS_ferror();
}

int CFileSystem_Stdio::FS_fflush(FILE *fp) {
  return ((IStdFilesystemFile *)fp)->FS_fflush();
}

char *CFileSystem_Stdio::FS_fgets(char *dest, int destSize, FILE *fp) {
  return ((IStdFilesystemFile *)fp)->FS_fgets(dest, destSize);
}

int CFileSystem_Stdio::FS_chmod(const char *path, int pmode) {
  int rt = path ? _chmod(path, pmode) : -1;

#ifndef OS_WIN
  if (rt == -1) {
    const char *file = findFileInDirCaseInsensitive(path);
    if (file) rt = _chmod(file, pmode);
  }
#endif

  return rt;
}

int CFileSystem_Stdio::FS_stat(const char *path, struct _stat *buf) {
  int rt = path ? _stat(path, buf) : -1;

#ifndef OS_WIN
  if (rt == -1) {
    const char *file = findFileInDirCaseInsensitive(path);
    if (file) rt = _stat(file, buf);
  }
#endif

  return rt;
}

int CFileSystem_Stdio::FS_fexists(const char *path) {
#ifdef OS_WIN
  const DWORD file_attributes{GetFileAttributes(path)};
  // Can be directory, too.
  return file_attributes != INVALID_FILE_ATTRIBUTES ? 0 : -1;
#elif defined(OS_POSIX)
  struct _stat buf;
  return FS_stat(path, &buf);
#else
#error Please, define FS_fexists at filessytem/filesystem_stdio.cpp
#endif
}

HANDLE CFileSystem_Stdio::FS_FindFirstFile(const char *findname,
                                           WIN32_FIND_DATA *dat) {
  return ::FindFirstFile(findname, dat);
}

bool CFileSystem_Stdio::FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat) {
  return ::FindNextFile(handle, dat) != 0;
}

bool CFileSystem_Stdio::FS_FindClose(HANDLE handle) {
  return ::FindClose(handle) != 0;
}

int CFileSystem_Stdio::FS_GetSectorSize(FILE *fp) {
  return ((IStdFilesystemFile *)fp)->FS_GetSectorSize();
}

// Files are always immediately available on disk.
bool CFileSystem_Stdio::IsFileImmediatelyAvailable(const char *pFileName) {
  return true;
}

// Steam call, unnecessary in stdio.
WaitForResourcesHandle_t CFileSystem_Stdio::WaitForResources(
    const char *resourcelist) {
  return 1;
}

// Purpose: steam call, unnecessary in stdio
bool CFileSystem_Stdio::GetWaitForResourcesProgress(
    WaitForResourcesHandle_t handle, float *progress /* out */,
    bool *complete /* out */) {
  // always return that we're complete
  *progress = 0.0f;
  *complete = true;
  return true;
}

// Purpose: steam call, unnecessary in stdio
void CFileSystem_Stdio::CancelWaitForResources(
    WaitForResourcesHandle_t handle) {}

void CFileSystem_Stdio::GetLocalCopy(const char *pFileName) {
  // do nothing. . everything is local.
}

int CFileSystem_Stdio::HintResourceNeed(const char *hintlist,
                                        int forgetEverything) {
  // do nothing. . everything is local.
  return 0;
}

class CThreadIOEventPool {
 public:
  ~CThreadIOEventPool() {
    CThreadEvent *pEvent;

    while (m_Events.PopItem(&pEvent)) {
      delete pEvent;
    }
  }

  CThreadEvent *GetEvent() {
    CThreadEvent *pEvent;

    if (m_Events.PopItem(&pEvent)) {
      return pEvent;
    }

    return new CThreadEvent;
  }

  void ReleaseEvent(CThreadEvent *pEvent) { m_Events.PushItem(pEvent); }

 private:
  CTSList<CThreadEvent *> m_Events;
};

CThreadIOEventPool g_ThreadIOEvents;

size_t Win32ReadOnlyFile::FS_fread(void *dest, size_t destSize, size_t size) {
  VPROF_BUDGET("CWin32ReadOnlyFile::FS_fread",
               VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  if (!size || (unbuffered_file_ == INVALID_HANDLE_VALUE &&
                buffered_file_ == INVALID_HANDLE_VALUE)) {
    return 0;
  }

  if (destSize == std::numeric_limits<size_t>::max()) destSize = size;

  // Ends up on a thread's stack
  u8 tempBuffer[512 * 1024];
  HANDLE hReadFile = buffered_file_;
  size_t nBytesToRead = size;
  u8 *pDest = (u8 *)dest;
  int64_t offset = read_offset_;

  if (unbuffered_file_ != INVALID_HANDLE_VALUE) {
    const unsigned long destBaseAlign = sector_size_;
    bool bDestBaseIsAligned = ((DWORD)(uintptr_t)dest % destBaseAlign == 0);
    bool bCanReadUnbufferedDirect =
        (bDestBaseIsAligned && (destSize % sector_size_ == 0) &&
         (read_offset_ % sector_size_ == 0));

    if (bCanReadUnbufferedDirect) {
      // fastest path, unbuffered
      nBytesToRead = AlignValue(size, sector_size_);
      hReadFile = unbuffered_file_;
    } else {
      // not properly aligned, snap to alignments
      // attempt to perform single unbuffered operation using stack buffer
      int64_t alignedOffset =
          AlignValue((read_offset_ - sector_size_) + 1, sector_size_);
      size_t alignedBytesToRead =
          AlignValue((read_offset_ - alignedOffset) + size, sector_size_);
      if (alignedBytesToRead <= sizeof(tempBuffer) - destBaseAlign) {
        // read operation can be performed as unbuffered follwed by a post
        // fixup
        nBytesToRead = alignedBytesToRead;
        offset = alignedOffset;
        pDest = AlignValue(tempBuffer, destBaseAlign);
        hReadFile = unbuffered_file_;
      }
    }
  }

  CThreadEvent *pEvent = nullptr;
  OVERLAPPED overlapped = {0};
  if (is_overlapped_) {
    pEvent = g_ThreadIOEvents.GetEvent();
    overlapped.hEvent = *pEvent;
  }

#ifdef REPORT_BUFFERED_IO
  if (hReadFile == buffered_file_ && filesystem_report_buffered_io.GetBool()) {
    Msg("Buffered Operation :(\n");
  }
#endif

  // some disk drivers will fail if read is too large
  static size_t kMaxReadBytes =
      filesystem_max_stdio_read.GetInt() * 1024 * 1024;
  const size_t kMinReadBytes{64 * 1024};

  bool bReadOk = true;
  DWORD nBytesRead = 0;
  size_t result = 0;
  int64_t currentOffset = offset;

  while (bReadOk && nBytesToRead > 0) {
    size_t nCurBytesToRead = std::min(nBytesToRead, kMaxReadBytes);
    DWORD nCurBytesRead = 0;

    overlapped.Offset = currentOffset & 0xFFFFFFFF;
    overlapped.OffsetHigh = (currentOffset >> 32) & 0xFFFFFFFF;

    bReadOk = ::ReadFile(hReadFile, pDest + nBytesRead, nCurBytesToRead,
                         &nCurBytesRead, &overlapped) != 0;
    if (!bReadOk) {
      if (is_overlapped_ && GetLastError() == ERROR_IO_PENDING) {
        bReadOk = true;
      }
    }

    if (bReadOk) {
      if (GetOverlappedResult(hReadFile, &overlapped, &nCurBytesRead, true)) {
        nBytesRead += nCurBytesRead;
        nBytesToRead -= nCurBytesToRead;
        currentOffset += nCurBytesRead;
      } else {
        bReadOk = false;
      }
    }

    if (!bReadOk) {
      DWORD dwError = GetLastError();

      if (dwError == ERROR_NO_SYSTEM_RESOURCES &&
          kMaxReadBytes > kMinReadBytes) {
        kMaxReadBytes /= 2;
        bReadOk = true;
        DevMsg("ERROR_NO_SYSTEM_RESOURCES: Reducing max read to %zu bytes\n",
               kMaxReadBytes);
      } else {
        DevMsg("Unknown read error %d\n", dwError);
      }
    }
  }

  if (bReadOk) {
    if (nBytesRead && hReadFile == unbuffered_file_ && pDest != dest) {
      int nBytesExtra = (read_offset_ - offset);
      nBytesRead -= nBytesExtra;
      if (nBytesRead) {
        memcpy(dest, (u8 *)pDest + nBytesExtra, size);
      }
    }

    result = std::min((size_t)nBytesRead, size);
  }

  if (is_overlapped_ && pEvent) {
    pEvent->Reset();
    g_ThreadIOEvents.ReleaseEvent(pEvent);
  }

  read_offset_ += result;

  return result;
}

#endif
