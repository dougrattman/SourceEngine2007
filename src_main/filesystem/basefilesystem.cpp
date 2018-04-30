// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "build/include/build_config.h"

#ifdef OS_POSIX
#define _snwprintf swprintf
#endif

#include "basefilesystem.h"

#include "base/include/posix_errno_info.h"
#include "filesystem/IQueuedLoader.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vprof.h"
#include "tier1/KeyValues.h"
#include "tier1/characterset.h"
#include "tier1/convar.h"
#include "tier1/generichash.h"
#include "tier1/lzmaDecoder.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier2/tier2.h"
#include "zip_utils.h"

#ifndef DEDICATED
#include "keyvaluescompiler.h"
#endif
#include "ifilelist.h"

#ifdef OS_WIN
// Needed for getting file type string
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#endif

#include "tier0/include/memdbgon.h"

ConVar fs_report_sync_opens("fs_report_sync_opens", "0", 0,
                            "0:Off, 1:Always, 2:Not during load");
ConVar fs_warning_mode("fs_warning_mode", "0", 0,
                       "0:Off, 1:Warn main thread, 2:Warn other threads");
ConVar fs_monitor_read_from_pack("fs_monitor_read_from_pack", "0", 0,
                                 "0:Off, 1:Any, 2:Sync only");

#define BSPOUTPUT \
  0  // bsp output flag -- determines type of fs_log output to generate

static void AddSeperatorAndFixPath(char *str);

// Case-insensitive symbol table for path IDs.
CUtlSymbolTableMT g_PathIDTable(0, 32, true);

int g_iNextSearchPathID = 1;

// Look for cases like materials\\blah.vmt.
bool V_CheckDoubleSlashes(const char *pStr) {
  usize len = strlen(pStr);

  for (usize i = 1; i < len - 1; i++) {
    if ((pStr[i] == '/' || pStr[i] == '\\') &&
        (pStr[i + 1] == '/' || pStr[i + 1] == '\\'))
      return true;
  }
  return false;
}

// This can be used to easily fix a filename on the stack.
#define CHECK_DOUBLE_SLASHES(x) Assert(V_CheckDoubleSlashes(x) == false);

// Win32 dedicated.dll contains both filesystem_steam.cpp and
// filesystem_stdio.cpp, so it has two CBaseFileSystem objects.  We'll let it
// manage BaseFileSystem() itself.
#if !(defined(_WIN32) && defined(DEDICATED))
static CBaseFileSystem *g_pBaseFileSystem;
CBaseFileSystem *BaseFileSystem() { return g_pBaseFileSystem; }
#endif

ConVar filesystem_buffer_size("filesystem_buffer_size", "0", 0,
                              "Size of per file buffers. 0 for none");

#if defined(TRACK_BLOCKING_IO)

// If we hit more than 100 items in a frame, we're probably doing a level
// load...
#define MAX_ITEMS 100

class CBlockingFileItemList : public IBlockingFileItemList {
 public:
  CBlockingFileItemList(CBaseFileSystem *fs) : m_pFS(fs), m_bLocked(false) {}

  // You can't call any of the below calls without calling these methods!!!!
  virtual void LockMutex() {
    Assert(!m_bLocked);
    if (m_bLocked) return;
    m_bLocked = true;
    m_pFS->BlockingFileAccess_EnterCriticalSection();
  }

  virtual void UnlockMutex() {
    Assert(m_bLocked);
    if (!m_bLocked) return;

    m_pFS->BlockingFileAccess_LeaveCriticalSection();
    m_bLocked = false;
  }

  virtual int First() const {
    if (!m_bLocked) {
      Error(
          "CBlockingFileItemList::First() w/o calling "
          "EnterCriticalSectionFirst!");
    }
    return m_Items.Head();
  }

  virtual int Next(int i) const {
    if (!m_bLocked) {
      Error(
          "CBlockingFileItemList::Next() w/o calling "
          "EnterCriticalSectionFirst!");
    }
    return m_Items.Next(i);
  }

  virtual int InvalidIndex() const { return m_Items.InvalidIndex(); }

  virtual const FileBlockingItem &Get(int index) const {
    if (!m_bLocked) {
      Error(
          "CBlockingFileItemList::Get( %d ) w/o calling "
          "EnterCriticalSectionFirst!",
          index);
    }
    return m_Items[index];
  }

  virtual void Reset() {
    if (!m_bLocked) {
      Error(
          "CBlockingFileItemList::Reset() w/o calling "
          "EnterCriticalSectionFirst!");
    }
    m_Items.RemoveAll();
  }

  void Add(const FileBlockingItem &item) {
    // Ack, should use a linked list probably...
    while (m_Items.Count() > MAX_ITEMS) {
      m_Items.Remove(m_Items.Head());
    }
    m_Items.AddToTail(item);
  }

 private:
  CUtlLinkedList<FileBlockingItem, unsigned short> m_Items;
  CBaseFileSystem *m_pFS;
  bool m_bLocked;
};
#endif

CUtlSymbol CBaseFileSystem::m_GamePathID;
CUtlSymbol CBaseFileSystem::m_BSPPathID;
DVDMode_t CBaseFileSystem::m_DVDMode;
CUtlVector<FileNameHandle_t> CBaseFileSystem::m_ExcludePaths;

class CStoreIDEntry {
 public:
  CStoreIDEntry() {}
  CStoreIDEntry(const char *pPathIDStr, int storeID)
      : m_PathIDString{pPathIDStr}, m_StoreID{storeID} {}

 public:
  CUtlSymbol m_PathIDString;
  int m_StoreID;
};

// CSimpleFileList (used by CFileCRCTracker).  Uses a dictionary to refer to the
// list of files.
class CFileSystemReloadFileList : public IFileList {
 public:
  CFileSystemReloadFileList(CBaseFileSystem *pFileSystem) {
    m_pFileSystem = pFileSystem;
  }

  virtual void Release() { delete this; }

  // The engine is calling this for any files it wants to be pure.
  // Return true if this file should be reloaded based on its current state and
  // the whitelist that we have now.
  virtual bool IsFileInList(const char *pFilename) {
    bool bRet = m_pFileSystem->ShouldGameReloadFile(pFilename);
    return bRet;
  }

 private:
  CBaseFileSystem *m_pFileSystem;
};

static CStoreIDEntry *FindPrevFileByStoreID(
    CUtlDict<CUtlVector<CStoreIDEntry> *, int> &filesByStoreID,
    const char *pFilename, const char *pPathIDStr, int foundStoreID) {
  int iEntry = filesByStoreID.Find(pFilename);

  if (iEntry == filesByStoreID.InvalidIndex()) {
    CUtlVector<CStoreIDEntry> *pList = new CUtlVector<CStoreIDEntry>;
    pList->AddToTail(CStoreIDEntry(pPathIDStr, foundStoreID));
    filesByStoreID.Insert(pFilename, pList);
    return nullptr;
  }

  // Now is there a previous entry with a different path ID string and the
  // same store ID?
  CUtlVector<CStoreIDEntry> *pList = filesByStoreID[iEntry];
  for (int i = 0; i < pList->Count(); i++) {
    CStoreIDEntry &entry = pList->Element(i);
    if (entry.m_StoreID == foundStoreID &&
        _stricmp(entry.m_PathIDString.String(), pPathIDStr) != 0)
      return &entry;
  }

  return nullptr;
}

CBaseFileSystem::CBaseFileSystem()
    : m_FileTracker(this), m_FileWhitelist(nullptr) {
#if !(defined(_WIN32) && defined(DEDICATED))
  g_pBaseFileSystem = this;
#endif
  g_pFullFileSystem = this;

  m_WhitelistFileTrackingEnabled = -1;

  // If this changes then FileNameHandleInternal_t/FileNameHandle_t needs to be
  // fixed!!!
  static_assert(sizeof(CUtlSymbol) == sizeof(short));

  // Clear out statistics
  memset(&m_Stats, 0, sizeof(m_Stats));

  m_fwLevel = FILESYSTEM_WARNING_REPORTUNCLOSED;
  m_pfnWarning = nullptr;
  m_pLogFile = nullptr;
  m_bOutputDebugString = false;
  m_WhitelistSpewFlags = 0;
  m_DirtyDiskReportFunc = nullptr;

  m_pThreadPool = nullptr;
#if defined(TRACK_BLOCKING_IO)
  m_pBlockingItems = new CBlockingFileItemList(this);
  m_bBlockingFileAccessReportingEnabled = false;
  m_bAllowSynchronousLogging = true;
#endif

  m_iMapLoad = 0;

  memset(m_PreloadData, 0, sizeof(m_PreloadData));

  // allows very specifc constrained behavior
  m_DVDMode = DVDMODE_OFF;
}

CBaseFileSystem::~CBaseFileSystem() {
  m_PathIDInfos.PurgeAndDeleteElements();
#if defined(TRACK_BLOCKING_IO)
  delete m_pBlockingItems;
#endif

  // Free the whitelist.
  RegisterFileWhitelist(nullptr, nullptr, nullptr);
}

void *CBaseFileSystem::QueryInterface(const char *pInterfaceName) {
  // We also implement the IMatSystemSurface interface
  if (!strncmp(pInterfaceName, BASEFILESYSTEM_INTERFACE_VERSION,
               strlen(BASEFILESYSTEM_INTERFACE_VERSION) + 1))
    return implicit_cast<IBaseFileSystem *>(this);

  return nullptr;
}

InitReturnVal_t CBaseFileSystem::Init() {
  InitReturnVal_t nRetVal = BaseClass::Init();
  if (nRetVal != INIT_OK) return nRetVal;

  // This is a special tag to allow iterating just the BSP file, it doesn't show
  // up in the list per se, but gets converted to "GAME" in the filter function
  m_BSPPathID = g_PathIDTable.AddString("BSP");
  m_GamePathID = g_PathIDTable.AddString("GAME");

  char fs_debug_env[512];
  size_t fs_debug_env_size;

  if (!getenv_s(&fs_debug_env_size, fs_debug_env, "fs_debug") &&
      fs_debug_env_size > 0) {
    m_bOutputDebugString = true;
  }

  const char *log_file_name = CommandLine()->ParmValue("-fs_log");
  if (log_file_name) {
    // STEAM OK
    if (!fopen_s(&m_pLogFile, log_file_name, "w")) {
      fprintf_s(m_pLogFile, "@echo off\n");
      fprintf_s(m_pLogFile, "setlocal\n");

      const char *fs_target = CommandLine()->ParmValue("-fs_target");
      if (fs_target) {
        fprintf_s(m_pLogFile, "set fs_target=\"%s\"\n", fs_target);
      }

      fprintf_s(m_pLogFile, "if \"%%fs_target%%\" == \"\" goto error\n");
      fprintf_s(m_pLogFile, "@echo on\n");
    } else {
      return INIT_FAILED;
    }
  }

  InitAsync();

  return INIT_OK;
}

void CBaseFileSystem::Shutdown() {
  ShutdownAsync();

  if (m_pLogFile) {
    if (CommandLine()->FindParm("-fs_logbins") > 0) {
      char cwd[MAX_FILEPATH];
      _getcwd(cwd, std::size(cwd));
      fprintf(m_pLogFile, "set binsrc=\"%s\"\n", cwd);
      fprintf(m_pLogFile, "mkdir \"%%fs_target%%\"\n");
      fprintf(m_pLogFile, "copy \"%%binsrc%%\\hl2.exe\" \"%%fs_target%%\"\n");
      fprintf(m_pLogFile, "copy \"%%binsrc%%\\hl2.dat\" \"%%fs_target%%\"\n");
      fprintf(m_pLogFile, "mkdir \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\*.asi\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\materialsystem.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\shaderapidx9.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\filesystem_stdio.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\soundemittersystem.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(
          m_pLogFile,
          "copy \"%%binsrc%%\\bin\\stdshader*.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(
          m_pLogFile,
          "copy \"%%binsrc%%\\bin\\shader_nv*.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(
          m_pLogFile,
          "copy \"%%binsrc%%\\bin\\launcher.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\engine.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\mss32.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\tier0.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\vgui2.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\vguimatsurface.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(
          m_pLogFile,
          "copy \"%%binsrc%%\\bin\\voice_miles.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(
          m_pLogFile,
          "copy \"%%binsrc%%\\bin\\vphysics.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\vstdlib.dll\" \"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\studiorender.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\bin\\vaudio_miles.dll\" "
              "\"%%fs_target%%\\bin\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\hl2\\resource\\*.ttf\" "
              "\"%%fs_target%%\\hl2\\resource\"\n");
      fprintf(m_pLogFile,
              "copy \"%%binsrc%%\\hl2\\bin\\gameui.dll\" "
              "\"%%fs_target%%\\hl2\\bin\"\n");
    }
    fprintf(m_pLogFile, "goto done\n");
    fprintf(m_pLogFile, ":error\n");
    fprintf(m_pLogFile,
            "echo "
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
            "!!!\"\n");
    fprintf(m_pLogFile,
            "echo ERROR: must set fs_target=targetpath (ie. \"set "
            "fs_target=u:\\destdir\")!\n");
    fprintf(m_pLogFile,
            "echo "
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
            "!!!\"\n");
    fprintf(m_pLogFile, ":done\n");
    fclose(m_pLogFile);  // STEAM OK
  }

  UnloadCompiledKeyValues();

  RemoveAllSearchPaths();
  Trace_DumpUnclosedFiles();
  BaseClass::Shutdown();
}

// Computes a full write path
inline void CBaseFileSystem::ComputeFullWritePath(char *pDest, int maxlen,
                                                  const char *pRelativePath,
                                                  const char *pWritePathID) {
  strcpy_s(pDest, maxlen, GetWritePath(pRelativePath, pWritePathID));
  strcat_s(pDest, maxlen, pRelativePath);
  Q_FixSlashes(pDest);
}

bool CBaseFileSystem::OpenedFileLessFunc(COpenedFile const &src1,
                                         COpenedFile const &src2) {
  return src1.m_pFile < src2.m_pFile;
}

void CBaseFileSystem::InstallDirtyDiskReportFunc(FSDirtyDiskReportFunc_t func) {
  m_DirtyDiskReportFunc = func;
}

void CBaseFileSystem::LogAccessToFile(char const *accesstype,
                                      char const *fullpath,
                                      char const *options) {
  LOCAL_THREAD_LOCK();

  if (m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES) {
    Warning(FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS%s:  %s %s (%.3f)\n",
            ThreadInMainThread() ? "" : "[a]", accesstype, fullpath,
            Plat_FloatTime());
  }

  int c = m_LogFuncs.Count();
  if (!c) return;

  for (int i = 0; i < c; ++i) {
    (m_LogFuncs[i])(fullpath, options);
  }
}

FILE *CBaseFileSystem::Trace_FOpen(const char *filename, const char *options,
                                   unsigned flags, int64_t *size,
                                   CFileLoadInfo *pInfo) {
  AUTOBLOCKREPORTER_FN(Trace_FOpen, this, true, filename,
                       FILESYSTEM_BLOCKING_SYNCHRONOUS,
                       FileBlockingItem::FB_ACCESS_OPEN);

  FILE *fp = FS_fopen(filename, options, flags, size, pInfo);
  if (fp) {
    FS_setbufsize(
        fp, options[0] == 'r' ? filesystem_buffer_size.GetInt() : 32 * 1024);

    AUTO_LOCK(m_OpenedFilesMutex);
    COpenedFile file;

    file.SetName(filename);
    file.m_pFile = fp;

    m_OpenedFiles.AddToTail(file);

    LogAccessToFile("open", filename, options);
  } else {
    if (m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES) {
      Warning(FILESYSTEM_WARNING_REPORTALLACCESSES,
              "Tried to open %s with options %s, but failed: %s\n", filename,
              options, source::posix_errno_info_last_error().description);
    }
  }

  return fp;
}

void CBaseFileSystem::GetFileNameForHandle(FileHandle_t handle, char *buf,
                                           size_t buflen) {
  strcpy_s(buf, buflen, "Unknown");
}

void CBaseFileSystem::Trace_FClose(FILE *fp) {
  if (fp) {
    m_OpenedFilesMutex.Lock();

    COpenedFile file;
    file.m_pFile = fp;

    int result = m_OpenedFiles.Find(file);
    if (result != -1 /*m_OpenedFiles.InvalidIdx()*/) {
      if (m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES) {
        COpenedFile found = m_OpenedFiles[result];
        Warning(FILESYSTEM_WARNING_REPORTALLACCESSES,
                "---FS%s:  close %s %p %i (%.3f)\n",
                ThreadInMainThread() ? "" : "[a]", found.GetName(), fp,
                m_OpenedFiles.Count(), Plat_FloatTime());
      }
      m_OpenedFiles.Remove(result);
    } else {
      Assert(0);

      if (m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES) {
        Warning(FILESYSTEM_WARNING_REPORTALLACCESSES,
                "Tried to close unknown file pointer %p\n", fp);
      }
    }

    m_OpenedFilesMutex.Unlock();

    FS_fclose(fp);
  }
}

void CBaseFileSystem::Trace_FRead(int size, FILE *fp) {
  if (!fp || m_fwLevel < FILESYSTEM_WARNING_REPORTALLACCESSES_READ) return;

  AUTO_LOCK(m_OpenedFilesMutex);

  COpenedFile file;
  file.m_pFile = fp;

  int result = m_OpenedFiles.Find(file);

  if (result != -1) {
    COpenedFile found = m_OpenedFiles[result];

    Warning(FILESYSTEM_WARNING_REPORTALLACCESSES_READ,
            "---FS%s:  read %s %i %p (%.3f)\n",
            ThreadInMainThread() ? "" : "[a]", found.GetName(), size, fp,
            Plat_FloatTime());
  } else {
    Warning(FILESYSTEM_WARNING_REPORTALLACCESSES_READ,
            "Tried to read %i bytes from unknown file pointer %p\n", size, fp);
  }
}

void CBaseFileSystem::Trace_FWrite(int size, FILE *fp) {
  if (!fp || m_fwLevel < FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE) return;

  COpenedFile file;
  file.m_pFile = fp;

  AUTO_LOCK(m_OpenedFilesMutex);

  int result = m_OpenedFiles.Find(file);

  if (result != -1) {
    COpenedFile found = m_OpenedFiles[result];

    Warning(FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE,
            "---FS%s:  write %s %i %p\n", ThreadInMainThread() ? "" : "[a]",
            found.GetName(), size, fp);
  } else {
    Warning(FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE,
            "Tried to write %i bytes from unknown file pointer %p\n", size, fp);
  }
}

void CBaseFileSystem::Trace_DumpUnclosedFiles() {
  AUTO_LOCK(m_OpenedFilesMutex);
  for (int i = 0; i < m_OpenedFiles.Count(); i++) {
    COpenedFile *found = &m_OpenedFiles[i];

    if (m_fwLevel >= FILESYSTEM_WARNING_REPORTUNCLOSED) {
      Warning(FILESYSTEM_WARNING_REPORTUNCLOSED, "File %s was never closed\n",
              found->GetName());
    }
  }
}

void CBaseFileSystem::PrintOpenedFiles() {
  FileWarningLevel_t saveLevel = m_fwLevel;
  m_fwLevel = FILESYSTEM_WARNING_REPORTUNCLOSED;
  Trace_DumpUnclosedFiles();
  m_fwLevel = saveLevel;
}

// Purpose: Adds the specified pack file to the list
bool CBaseFileSystem::AddPackFile(const char *pFileName, const char *pathID) {
  CHECK_DOUBLE_SLASHES(pFileName);

  AsyncFinishAll();
  return AddPackFileFromPath("", pFileName, true, pathID);
}

// Purpose: Adds a pack file from the specified path
bool CBaseFileSystem::AddPackFileFromPath(const char *pPath,
                                          const char *pakfile,
                                          bool bCheckForAppendedPack,
                                          const char *pathID) {
  char fullpath[SOURCE_MAX_PATH];
  sprintf_s(fullpath, "%s%s", pPath, pakfile);
  Q_FixSlashes(fullpath);

  struct _stat buf;
  if (FS_stat(fullpath, &buf) == -1) return false;

  CPackFile *pf = new CZipPackFile(this);
  pf->m_hPackFileHandle = Trace_FOpen(fullpath, "rb", 0, nullptr);

  // Get the length of the pack file:
  FS_fseek((FILE *)pf->m_hPackFileHandle, 0, FILESYSTEM_SEEK_TAIL);
  int64_t len = FS_ftell((FILE *)pf->m_hPackFileHandle);
  FS_fseek((FILE *)pf->m_hPackFileHandle, 0, FILESYSTEM_SEEK_HEAD);

  if (!pf->Prepare(len)) {
    // Failed for some reason, ignore it
    Trace_FClose(pf->m_hPackFileHandle);
    pf->m_hPackFileHandle = nullptr;
    delete pf;

    return false;
  }

  // Add this pack file to the search path:
  CSearchPath *sp = &m_SearchPaths[m_SearchPaths.AddToTail()];
  pf->SetPath(sp->GetPath());
  pf->m_lPackFileTime = GetFileTime(pakfile);

  sp->SetPath(pPath);
  sp->m_pPathIDInfo->SetPathID(pathID);
  sp->SetPackFile(pf);

  return true;
}

// Read a bit of the file from the pack file:
int CPackFileHandle::Read(void *pBuffer, int nDestSize, int nBytes) {
  // Clamp nBytes to not go past the end of the file (async is still possible
  // due to nDestSize)
  if (nBytes + m_nFilePointer > m_nLength) {
    nBytes = m_nLength - m_nFilePointer;
  }

  // Seek to the given file pointer and read
  int nBytesRead = m_pOwner->ReadFromPack(m_nIndex, pBuffer, nDestSize, nBytes,
                                          m_nBase + m_nFilePointer);

  m_nFilePointer += nBytesRead;

  return nBytesRead;
}

// Seek around inside the pack:
int CPackFileHandle::Seek(int nOffset, int nWhence) {
  if (nWhence == SEEK_SET) {
    m_nFilePointer = nOffset;
  } else if (nWhence == SEEK_CUR) {
    m_nFilePointer += nOffset;
  } else if (nWhence == SEEK_END) {
    m_nFilePointer = m_nLength + nOffset;
  }

  // Clamp the file pointer to the actual bounds of the file:
  if (m_nFilePointer > m_nLength) {
    m_nFilePointer = m_nLength;
  }

  return m_nFilePointer;
}

//-----------------------------------------------------------------------------
// Low Level I/O routine for reading from pack files.
// Offsets all reads by the base of the pack file as needed.
// Return bytes read.
//-----------------------------------------------------------------------------
int CPackFile::ReadFromPack(int nIndex, void *buffer, int nDestBytes,
                            int nBytes, int64_t nOffset) {
  m_mutex.Lock();

  if (fs_monitor_read_from_pack.GetInt() == 1 ||
      (fs_monitor_read_from_pack.GetInt() == 2 && ThreadInMainThread())) {
    // spew info about real i/o request
    char szName[SOURCE_MAX_PATH];
    IndexToFilename(nIndex, szName, sizeof(szName));
    Msg("Read From Pack: Sync I/O: Requested:%7d, Offset:0x%16.16x, %s\n",
        nBytes, m_nBaseOffset + nOffset, szName);
  }

  // Seek to the start of the read area and perform the read: TODO: CHANGE THIS
  // INTO A CFileHandle
  m_fs->FS_fseek(m_hPackFileHandle, m_nBaseOffset + nOffset, SEEK_SET);
  int nBytesRead =
      m_fs->FS_fread(buffer, nDestBytes, nBytes, m_hPackFileHandle);

  m_mutex.Unlock();

  return nBytesRead;
}

//-----------------------------------------------------------------------------
// Open a file inside of a pack file.
//-----------------------------------------------------------------------------
CFileHandle *CPackFile::OpenFile(const char *pFileName, const char *pOptions) {
  int nIndex, nLength;
  int64_t nPosition;

  // find the file's location in the pack
  if (FindFile(pFileName, nIndex, nPosition, nLength)) {
    m_mutex.Lock();
    if (m_nOpenFiles == 0 && m_hPackFileHandle == nullptr) {
      m_hPackFileHandle = m_fs->Trace_FOpen(m_ZipName, "rb", 0, nullptr);
    }
    m_nOpenFiles++;
    m_mutex.Unlock();
    CPackFileHandle *ph = new CPackFileHandle(this, nPosition, nLength, nIndex);
    CFileHandle *fh = new CFileHandle(m_fs);
    fh->m_pPackFileHandle = ph;
    fh->m_nLength = nLength;

    // The default mode for fopen is text, so require 'b' for binary
    if (strchr(pOptions, 'b') == nullptr) {
      fh->m_type = FT_PACK_TEXT;
    } else {
      fh->m_type = FT_PACK_BINARY;
    }

#if !defined(_RETAIL)
    fh->SetName(pFileName);
#endif
    return fh;
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
//	Get a directory entry from a pack's preload section
//-----------------------------------------------------------------------------
ZIP_PreloadDirectoryEntry *CZipPackFile::GetPreloadEntry(int nEntryIndex) {
  if (!m_pPreloadHeader) {
    return nullptr;
  }

  // If this entry doesn't have a corresponding preload entry, fail.
  if (m_PackFiles[nEntryIndex].m_nPreloadIdx == INVALID_PRELOAD_ENTRY) {
    return nullptr;
  }

  return &m_pPreloadDirectory[m_PackFiles[nEntryIndex].m_nPreloadIdx];
}

//-----------------------------------------------------------------------------
//	Read a file from the pack
//-----------------------------------------------------------------------------
int CZipPackFile::ReadFromPack(int nEntryIndex, void *pBuffer, int nDestBytes,
                               int nBytes, int64_t nOffset) {
  if (nEntryIndex >= 0) {
    if (nBytes <= 0) {
      return 0;
    }

    // X360TBD: This is screwy, it works because m_nBaseOffset is 0 for preload
    // capable zips It comes into play for files out of the embedded bsp zip,
    // this hackery is a pre-bias expecting ReadFromPack() do a symmetric post
    // bias, yuck.
    nOffset -= m_nBaseOffset;

    // Attempt to satisfy request from possible preload section, otherwise fall
    // through A preload entry may be compressed
    ZIP_PreloadDirectoryEntry *pPreloadEntry = GetPreloadEntry(nEntryIndex);
    if (pPreloadEntry) {
      // convert the absolute pack file position to a local file position
      int nLocalOffset = nOffset - m_PackFiles[nEntryIndex].m_nPosition;
      byte *pPreloadData = (byte *)m_pPreloadData + pPreloadEntry->DataOffset;

      CLZMA lzma;
      if (lzma.IsCompressed(pPreloadData)) {
        unsigned int actualSize = lzma.GetActualSize(pPreloadData);
        if (nLocalOffset + nBytes <= (int)actualSize) {
          // satisfy from compressed preload
          if (fs_monitor_read_from_pack.GetInt() == 1) {
            Msg("Read From Pack: [Preload] Requested:%d Compressed:%d\n",
                nBytes, pPreloadEntry->Length);
          }

          if (nLocalOffset == 0 && nDestBytes >= (int)actualSize &&
              nBytes == (int)actualSize) {
            // uncompress directly into caller's buffer
            lzma.Uncompress((unsigned char *)pPreloadData,
                            (unsigned char *)pBuffer);
            return nBytes;
          }

          // uncompress into temporary memory
          CUtlMemory<byte> tempMemory;
          tempMemory.EnsureCapacity(actualSize);
          lzma.Uncompress(pPreloadData, tempMemory.Base());
          // copy only what caller expects
          memcpy(pBuffer, (byte *)tempMemory.Base() + nLocalOffset, nBytes);
          return nBytes;
        }
      } else if (nLocalOffset + nBytes <= (int)pPreloadEntry->Length) {
        // satisfy from preload
        if (fs_monitor_read_from_pack.GetInt() == 1) {
          Msg("Read From Pack: [Preload] Requested:%d Total:%d\n", nBytes,
              pPreloadEntry->Length);
        }

        memcpy(pBuffer, pPreloadData + nLocalOffset, nBytes);
        return nBytes;
      }
    }
  }

  // fell through as a direct request
  return CPackFile::ReadFromPack(nEntryIndex, pBuffer, nDestBytes, nBytes,
                                 nOffset);
}

//-----------------------------------------------------------------------------
//	Gets size, position, and index for a file in the pack.
//-----------------------------------------------------------------------------
bool CZipPackFile::GetOffsetAndLength(const char *pFileName, int &nBaseIndex,
                                      int64_t &nFileOffset, int &nLength) {
  CZipPackFile::CPackFileEntry lookup;
  lookup.m_HashName = HashStringCaselessConventional(pFileName);

  int idx = m_PackFiles.Find(lookup);
  if (-1 != idx) {
    nFileOffset = m_PackFiles[idx].m_nPosition;
    nLength = m_PackFiles[idx].m_nLength;
    nBaseIndex = idx;
    return true;
  }

  return false;
}

bool CZipPackFile::IndexToFilename(int nIndex, char *pBuffer, int nBufferSize) {
#if !defined(_RETAIL)
  if (nIndex >= 0) {
    m_fs->String(m_PackFiles[nIndex].m_hDebugFilename, pBuffer, nBufferSize);
    return true;
  }
#endif

  strcpy_s(pBuffer, nBufferSize, "unknown");

  return false;
}

//-----------------------------------------------------------------------------
//	Find a file in the pack.
//-----------------------------------------------------------------------------
bool CZipPackFile::FindFile(const char *pFilename, int &nIndex,
                            int64_t &nOffset, int &nLength) {
  char szCleanName[MAX_FILEPATH];
  strcpy_s(szCleanName, pFilename);
#ifdef _WIN32
  Q_strlower(szCleanName);
#endif
  Q_FixSlashes(szCleanName);

  if (!Q_RemoveDotSlashes(szCleanName)) {
    return false;
  }

  bool bFound = GetOffsetAndLength(szCleanName, nIndex, nOffset, nLength);

  nOffset += m_nBaseOffset;
  return bFound;
}

//-----------------------------------------------------------------------------
//	Set up the preload section
//-----------------------------------------------------------------------------
void CZipPackFile::SetupPreloadData() {
  if (m_pPreloadHeader || !m_nPreloadSectionSize) {
    // already loaded or not availavble
    return;
  }

  MEM_ALLOC_CREDIT_("xZip");

  void *pPreload = malloc(m_nPreloadSectionSize);
  if (!pPreload) {
    return;
  }

  // preload data is loaded as a single unbuffered i/o operation
  ReadFromPack(-1, pPreload, -1, m_nPreloadSectionSize,
               m_nPreloadSectionOffset);

  // setup the header
  m_pPreloadHeader = (ZIP_PreloadHeader *)pPreload;

  // setup the preload directory
  m_pPreloadDirectory =
      (ZIP_PreloadDirectoryEntry *)((byte *)m_pPreloadHeader +
                                    sizeof(ZIP_PreloadHeader));

  // setup the remap table
  m_pPreloadRemapTable =
      (unsigned short *)((byte *)m_pPreloadDirectory +
                         m_pPreloadHeader->PreloadDirectoryEntries *
                             sizeof(ZIP_PreloadDirectoryEntry));

  // set the preload data base
  m_pPreloadData = (byte *)m_pPreloadRemapTable +
                   m_pPreloadHeader->DirectoryEntries * sizeof(unsigned short);
}

void CZipPackFile::DiscardPreloadData() {
  if (!m_pPreloadHeader) {
    // already discarded
    return;
  }

  free(m_pPreloadHeader);
  m_pPreloadHeader = nullptr;
}

//-----------------------------------------------------------------------------
//	Parse the zip file to build the file directory and preload section
//-----------------------------------------------------------------------------
bool CZipPackFile::Prepare(int64_t fileLen, int64_t nFileOfs) {
  if (!fileLen || fileLen < sizeof(ZIP_EndOfCentralDirRecord)) {
    // nonsense zip
    return false;
  }

  // Pack files are always little-endian
  m_swap.ActivateByteSwapping(IsX360());

  m_FileLength = fileLen;
  m_nBaseOffset = nFileOfs;

  ZIP_EndOfCentralDirRecord rec = {0};

  // Find and read the central header directory from its expected position at
  // end of the file
  bool bCentralDirRecord = false;
  int64_t offset = fileLen - sizeof(ZIP_EndOfCentralDirRecord);

  // scan entire file from expected location for central dir
  for (; offset >= 0; offset--) {
    ReadFromPack(-1, (void *)&rec, -1, sizeof(rec), offset);
    m_swap.SwapFieldsToTargetEndian(&rec);
    if (rec.signature == PKID(5, 6)) {
      bCentralDirRecord = true;
      break;
    }
  }

  Assert(bCentralDirRecord);
  if (!bCentralDirRecord) {
    // no zip directory, bad zip
    return false;
  }

  int numFilesInZip = rec.nCentralDirectoryEntries_Total;
  if (numFilesInZip <= 0) {
    // empty valid zip
    return true;
  }

  int firstFileIdx = 0;

  MEM_ALLOC_CREDIT();

  // read central directory into memory and parse
  CUtlBuffer zipDirBuff(0, rec.centralDirectorySize, 0);
  zipDirBuff.EnsureCapacity(rec.centralDirectorySize);
  zipDirBuff.ActivateByteSwapping(IsX360());
  ReadFromPack(-1, zipDirBuff.Base(), -1, rec.centralDirectorySize,
               rec.startOfCentralDirOffset);
  zipDirBuff.SeekPut(CUtlBuffer::SEEK_HEAD, rec.centralDirectorySize);

  ZIP_FileHeader zipFileHeader;
  char filename[SOURCE_MAX_PATH];

  // Check for a preload section, expected to be the first file in the zip
  zipDirBuff.GetObjects(&zipFileHeader);
  zipDirBuff.Get(filename, zipFileHeader.fileNameLength);
  filename[zipFileHeader.fileNameLength] = '\0';
  if (!_stricmp(filename, PRELOAD_SECTION_NAME)) {
    m_nPreloadSectionSize = zipFileHeader.uncompressedSize;
    m_nPreloadSectionOffset = zipFileHeader.relativeOffsetOfLocalHeader +
                              sizeof(ZIP_LocalFileHeader) +
                              zipFileHeader.fileNameLength +
                              zipFileHeader.extraFieldLength;
    SetupPreloadData();

    // Set up to extract the remaining files
    int nextOffset =
        zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
    zipDirBuff.SeekGet(CUtlBuffer::SEEK_CURRENT, nextOffset);
    firstFileIdx = 1;
  } else {
    // No preload section, reset buffer pointer
    zipDirBuff.SeekGet(CUtlBuffer::SEEK_HEAD, 0);
  }

  // Parse out central directory and determine absolute file positions of data.
  // Supports uncompressed zip files, with or without preload sections
  bool bSuccess = true;
  char tmpString[SOURCE_MAX_PATH];
  CZipPackFile::CPackFileEntry lookup;

  m_PackFiles.EnsureCapacity(numFilesInZip);

  for (int i = firstFileIdx; i < numFilesInZip; ++i) {
    zipDirBuff.GetObjects(&zipFileHeader);
    if (zipFileHeader.signature != PKID(1, 2) ||
        zipFileHeader.compressionMethod != 0) {
      Msg("Incompatible pack file detected! %s\n",
          (zipFileHeader.compressionMethod != 0) ? " File is compressed" : "");
      bSuccess = false;
      break;
    }

    Assert(zipFileHeader.fileNameLength < sizeof(tmpString));
    zipDirBuff.Get((void *)tmpString, zipFileHeader.fileNameLength);
    tmpString[zipFileHeader.fileNameLength] = '\0';
    Q_FixSlashes(tmpString);

#if !defined(_RETAIL)
    lookup.m_hDebugFilename = m_fs->FindOrAddFileName(tmpString);
#endif
    lookup.m_HashName = HashStringCaselessConventional(tmpString);
    lookup.m_nLength = zipFileHeader.uncompressedSize;
    lookup.m_nPosition = zipFileHeader.relativeOffsetOfLocalHeader +
                         sizeof(ZIP_LocalFileHeader) +
                         zipFileHeader.fileNameLength +
                         zipFileHeader.extraFieldLength;

    // track the index to this file's possible preload directory entry
    if (m_pPreloadRemapTable) {
      lookup.m_nPreloadIdx = m_pPreloadRemapTable[i];
    } else {
      lookup.m_nPreloadIdx = INVALID_PRELOAD_ENTRY;
    }
    m_PackFiles.InsertNoSort(lookup);

    int nextOffset =
        zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
    zipDirBuff.SeekGet(CUtlBuffer::SEEK_CURRENT, nextOffset);
  }

  m_PackFiles.RedoSort();

  return bSuccess;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CZipPackFile::CZipPackFile(CBaseFileSystem *fs) : m_PackFiles() {
  m_fs = fs;
  m_pPreloadDirectory = nullptr;
  m_pPreloadData = nullptr;
  m_pPreloadHeader = nullptr;
  m_pPreloadRemapTable = nullptr;
  m_nPreloadSectionOffset = 0;
  m_nPreloadSectionSize = 0;
}

CZipPackFile::~CZipPackFile() { DiscardPreloadData(); }

//-----------------------------------------------------------------------------
// Purpose:
// Input  : src1 -
//			src2 -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CZipPackFile::CPackFileLessFunc::Less(
    CZipPackFile::CPackFileEntry const &src1,
    CZipPackFile::CPackFileEntry const &src2, void *pCtx) {
  return (src1.m_HashName < src2.m_HashName);
}

//-----------------------------------------------------------------------------
// Purpose: Search pPath for pak?.pak files and add to search path if found
// Input  : *pPath -
//-----------------------------------------------------------------------------
#define PACK_NAME_FORMAT "zip%i.zip"

void CBaseFileSystem::AddPackFiles(const char *pPath, const CUtlSymbol &pathID,
                                   SearchPathAdd_t addType) {
  Assert(ThreadInMainThread());
  DISK_INTENSIVE();

  CUtlVector<CUtlString> pakNames;
  CUtlVector<int64_t> pakSizes;

  // determine pak files, [zip0..zipN]
  for (int i = 0;; i++) {
    char pakfile[SOURCE_MAX_PATH];
    char fullpath[SOURCE_MAX_PATH];
    sprintf_s(pakfile, PACK_NAME_FORMAT, i);
    V_ComposeFileName(pPath, pakfile, fullpath, sizeof(fullpath));

    struct _stat buf;
    if (FS_stat(fullpath, &buf) == -1) break;

    MEM_ALLOC_CREDIT();

    pakNames.AddToTail(pakfile);
    pakSizes.AddToTail((int64_t)((unsigned int)buf.st_size));
  }

  // Add any zip files in the format zip1.zip ... zip0.zip
  // Add them backwards so zip(N) is higher priority than zip(N-1), etc.
  int pakcount = pakSizes.Count();
  int nCount = 0;
  for (int i = pakcount - 1; i >= 0; i--) {
    char fullpath[SOURCE_MAX_PATH];
    V_ComposeFileName(pPath, pakNames[i].Get(), fullpath, sizeof(fullpath));

    int nIndex;
    if (addType == PATH_ADD_TO_TAIL) {
      nIndex = m_SearchPaths.AddToTail();
    } else {
      nIndex = m_SearchPaths.InsertBefore(nCount);
      ++nCount;
    }

    CSearchPath *sp = &m_SearchPaths[nIndex];

    sp->m_pPathIDInfo = FindOrAddPathIDInfo(pathID, -1);
    sp->m_storeId = g_iNextSearchPathID++;
    sp->SetPath(g_PathIDTable.AddString(pPath));

    CPackFile *pf = nullptr;
    for (int iPackFile = 0; iPackFile < m_ZipFiles.Count(); iPackFile++) {
      if (!_stricmp(m_ZipFiles[iPackFile]->m_ZipName.Get(), fullpath)) {
        pf = m_ZipFiles[iPackFile];
        sp->SetPackFile(pf);
        pf->AddRef();
      }
    }

    if (!pf) {
      pf = new CZipPackFile(this);
      pf->SetPath(sp->GetPath());

      MEM_ALLOC_CREDIT();
      pf->m_ZipName = fullpath;

      m_ZipFiles.AddToTail(pf);
      sp->SetPackFile(pf);
      pf->m_lPackFileTime = GetFileTime(fullpath);
      pf->m_hPackFileHandle = Trace_FOpen(fullpath, "rb", 0, nullptr);
      FS_setbufsize(pf->m_hPackFileHandle, 32 * 1024);  // 32k buffer.

      if (pf->Prepare(pakSizes[i])) {
        FS_setbufsize(pf->m_hPackFileHandle, filesystem_buffer_size.GetInt());
      } else {
        // Failed for some reason, ignore it
        if (pf->m_hPackFileHandle) {
          Trace_FClose(pf->m_hPackFileHandle);
          pf->m_hPackFileHandle = nullptr;
        }
        m_SearchPaths.Remove(nIndex);
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose: Wipe all map (.bsp) pak file search paths
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllMapSearchPaths() {
  AsyncFinishAll();

  int c = m_SearchPaths.Count();
  for (int i = c - 1; i >= 0; i--) {
    if (!(m_SearchPaths[i].GetPackFile() &&
          m_SearchPaths[i].GetPackFile()->m_bIsMapPath)) {
      continue;
    }

    m_SearchPaths.Remove(i);
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddMapPackFile(const char *pPath, const char *pPathID,
                                     SearchPathAdd_t addType) {
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pPath, pPathID, tempPathID);

  char newPath[MAX_FILEPATH];
  // +2 for '\0' and potential slash added at end.
  strcpy_s(newPath, pPath);
#ifdef _WIN32  // don't do this on linux!
  Q_strlower(newPath);
#endif
  Q_FixSlashes(newPath);

  // Open the .bsp and find the map lump
  char fullpath[MAX_FILEPATH];
  // If it's an absolute path, just use that.
  if (Q_IsAbsolutePath(newPath)) {
    strcpy_s(fullpath, newPath);
  } else {
    if (!GetLocalPath(newPath, fullpath, sizeof(fullpath))) {
      // Couldn't find that .bsp file!!!
      return;
    }
  }

  int c = m_SearchPaths.Count();
  for (int i = c - 1; i >= 0; i--) {
    if (!(m_SearchPaths[i].GetPackFile() &&
          m_SearchPaths[i].GetPackFile()->m_bIsMapPath))
      continue;

    if (_stricmp(m_SearchPaths[i].GetPackFile()->m_ZipName.Get(), fullpath) ==
        0) {
      // Already set as map path
      return;
    }
  }

  // Remove previous
  RemoveAllMapSearchPaths();

  FILE *fp = Trace_FOpen(fullpath, "rb", 0, nullptr);
  if (!fp) {
    // Couldn't open it
    Warning(FILESYSTEM_WARNING,
            "Couldn't open .bsp %s for embedded pack file check: %s.\n",
            fullpath, source::posix_errno_info_last_error().description);
    return;
  }

  // Get the .bsp file header
  dheader_t header;
  memset(&header, 0, sizeof(dheader_t));
  m_Stats.nBytesRead += FS_fread(&header, sizeof(header), fp);
  ++m_Stats.nReads;

  if (header.ident != IDBSPHEADER || header.version < MINBSPVERSION ||
      header.version > BSPVERSION) {
    Trace_FClose(fp);
    return;
  }

  // Find the LUMP_PAKFILE offset
  lump_t *packfile = &header.lumps[LUMP_PAKFILE];
  if (packfile->filelen <= sizeof(lump_t)) {
    // It's empty or only contains a file header ( so there are no entries ), so
    // don't add to search paths
    Trace_FClose(fp);
    return;
  }

  // Seek to correct position
  FS_fseek(fp, packfile->fileofs, FILESYSTEM_SEEK_HEAD);

  CPackFile *pf = new CZipPackFile(this);

  pf->m_bIsMapPath = true;
  pf->m_hPackFileHandle = fp;

  MEM_ALLOC_CREDIT();
  pf->m_ZipName = fullpath;

  if (pf->Prepare(packfile->filelen, packfile->fileofs)) {
    int nIndex;
    if (addType == PATH_ADD_TO_TAIL) {
      nIndex = m_SearchPaths.AddToTail();
    } else {
      nIndex = m_SearchPaths.AddToHead();
    }

    CSearchPath *sp = &m_SearchPaths[nIndex];

    sp->SetPackFile(pf);
    sp->m_storeId = g_iNextSearchPathID++;
    sp->SetPath(g_PathIDTable.AddString(newPath));
    sp->m_pPathIDInfo =
        FindOrAddPathIDInfo(g_PathIDTable.AddString(pPathID), -1);

    pf->SetPath(sp->GetPath());
    pf->m_lPackFileTime = GetFileTime(newPath);

    Trace_FClose(pf->m_hPackFileHandle);
    pf->m_hPackFileHandle = nullptr;

    m_ZipFiles.AddToTail(pf);
  } else {
    delete pf;
  }
}

void CBaseFileSystem::BeginMapAccess() {
  if (m_iMapLoad++ == 0) {
    int c = m_SearchPaths.Count();
    for (int i = 0; i < c; i++) {
      CSearchPath *pSearchPath = &m_SearchPaths[i];
      CPackFile *pPackFile = pSearchPath->GetPackFile();

      if (pPackFile && pPackFile->m_bIsMapPath) {
        pPackFile->AddRef();
        pPackFile->m_mutex.Lock();
        if (pPackFile->m_nOpenFiles == 0 &&
            pPackFile->m_hPackFileHandle == nullptr) {
          pPackFile->m_hPackFileHandle =
              Trace_FOpen(pPackFile->m_ZipName, "rb", 0, nullptr);
        }
        pPackFile->m_nOpenFiles++;
        pPackFile->m_mutex.Unlock();
      }
    }
  }
}

void CBaseFileSystem::EndMapAccess() {
  if (m_iMapLoad-- == 1) {
    int c = m_SearchPaths.Count();
    for (int i = 0; i < c; i++) {
      CSearchPath *pSearchPath = &m_SearchPaths[i];
      CPackFile *pPackFile = pSearchPath->GetPackFile();

      if (pPackFile && pPackFile->m_bIsMapPath) {
        pPackFile->m_mutex.Lock();
        pPackFile->m_nOpenFiles--;
        if (pPackFile->m_nOpenFiles == 0) {
          Trace_FClose(pPackFile->m_hPackFileHandle);
          pPackFile->m_hPackFileHandle = nullptr;
        }
        pPackFile->m_mutex.Unlock();
        pPackFile->Release();
      }
    }
  }
}

void CBaseFileSystem::PrintSearchPaths() {
  int i;
  Msg("---------------\n");
  Msg("Paths:\n");
  int c = m_SearchPaths.Count();
  for (i = 0; i < c; i++) {
    CSearchPath *pSearchPath = &m_SearchPaths[i];

    const char *pszPack = "";
    const char *pszType = "";
    if (pSearchPath->GetPackFile() &&
        pSearchPath->GetPackFile()->m_bIsMapPath) {
      pszType = "(map)";
    } else if (pSearchPath->GetPackFile()) {
      pszType = "(pack) ";
      pszPack = pSearchPath->GetPackFile()->m_ZipName;
    }

    Msg("\"%s\" \"%s\" %s%s\n", pSearchPath->GetPathString(),
        (const char *)pSearchPath->GetPathIDString(), pszType, pszPack);
  }
}

//-----------------------------------------------------------------------------
// Purpose: This is where search paths are created.  map files are created at
// head of list (they occur after
//  file system paths have already been set ) so they get highest priority.
//  Otherwise, we add the disk (non-packfile) path and then the paks if they
//  exist for the path
// Input  : *pPath -
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddSearchPathInternal(const char *pPath,
                                            const char *pathID,
                                            SearchPathAdd_t addType,
                                            bool bAddPackFiles) {
  AsyncFinishAll();

  Assert(ThreadInMainThread());

  // Map pak files have their own handler
  if (V_stristr(pPath, ".bsp")) {
    AddMapPackFile(pPath, pathID, addType);
    return;
  }

  // Clean up the name
  char newPath[MAX_FILEPATH];
  if (pPath[0] == 0) {
    newPath[0] = newPath[1] = 0;
  } else {
    if (Q_IsAbsolutePath(pPath)) {
      strcpy_s(newPath, pPath);
    } else {
      Q_MakeAbsolutePath(newPath, sizeof(newPath), pPath);
    }
#ifdef _WIN32
    Q_strlower(newPath);
#endif
    AddSeperatorAndFixPath(newPath);
  }

  // Make sure that it doesn't already exist
  CUtlSymbol pathSym, pathIDSym;
  pathSym = g_PathIDTable.AddString(newPath);
  pathIDSym = g_PathIDTable.AddString(pathID);
  int i;
  int c = m_SearchPaths.Count();
  int id = 0;
  for (i = 0; i < c; i++) {
    CSearchPath *pSearchPath = &m_SearchPaths[i];
    if (pSearchPath->GetPath() == pathSym &&
        pSearchPath->GetPathID() == pathIDSym) {
      if ((addType == PATH_ADD_TO_HEAD && i == 0) ||
          (addType == PATH_ADD_TO_TAIL)) {
        return;  // this entry is already at the head
      } else {
        m_SearchPaths.Remove(i);  // remove it from its current position so we
                                  // can add it back to the head
        i--;
        c--;
      }
    }
    if (!id && pSearchPath->GetPath() == pathSym) {
      // get first found - all reference the same store
      id = pSearchPath->m_storeId;
    }
  }

  if (!id) {
    id = g_iNextSearchPathID++;
  }

  // Add to list
  bool bAdded = false;
  int nIndex = m_SearchPaths.Count();
  if (bAddPackFiles) {
    // Add pack files for this path next
    AddPackFiles(newPath, pathIDSym, addType);
    bAdded = m_SearchPaths.Count() != nIndex;
  }
  if (addType == PATH_ADD_TO_HEAD) {
    nIndex = m_SearchPaths.Count() - nIndex;
    Assert(nIndex >= 0);
  }

  // Grab last entry and set the path
  m_SearchPaths.InsertBefore(nIndex);

  CSearchPath *sp = &m_SearchPaths[nIndex];

  sp->SetPath(pathSym);
  sp->m_pPathIDInfo = FindOrAddPathIDInfo(pathIDSym, -1);

  // all matching paths have a reference to the same store
  sp->m_storeId = id;
}

//-----------------------------------------------------------------------------
// Create the search path.
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddSearchPath(const char *pPath, const char *pathID,
                                    SearchPathAdd_t addType) {
  int currCount = m_SearchPaths.Count();

  AddSearchPathInternal(pPath, pathID, addType, true);

  if (currCount != m_SearchPaths.Count()) {
#if !defined(DEDICATED)
    if (IsDebug()) {
      // spew updated search paths
      PrintSearchPaths();
    }
#endif
  }
}

//-----------------------------------------------------------------------------
// Returns the search path, each path is separated by ;s. Returns the length of
// the string returned Pack search paths include the pack name, so that callers
// can still form absolute paths and that absolute path can be sent to the
// filesystem, and mounted as a file inside a pack.
//-----------------------------------------------------------------------------
int CBaseFileSystem::GetSearchPath(const char *pathID, bool bGetPackFiles,
                                   char *pPath, int nMaxLen) {
  AUTO_LOCK(m_SearchPathsMutex);

  int length = 0;
  if (nMaxLen) pPath[0] = '\0';

  CSearchPathsIterator it{this, pathID,
                          bGetPackFiles ? FILTER_NONE : FILTER_CULLPACK};
  for (auto *search_path = it.GetFirst(); search_path != nullptr;
       search_path = it.GetNext()) {
    CPackFile *pack_file = search_path->GetPackFile();

    if (length >= nMaxLen) {
      // Add 1 for the semicolon if our length is not 0
      length += (length > 0) ? 1 : 0;

      if (!pack_file) {
        length += strlen(search_path->GetPathString());
      } else {
        // full path and slash
        length += strlen(pack_file->m_ZipName.String()) + 1;
      }
      continue;
    }

    if (length != 0) pPath[length++] = ';';

    if (!pack_file) {
      strcpy_s(&pPath[length], nMaxLen - length, search_path->GetPathString());
      length += strlen(search_path->GetPathString());
    } else {
      // full path and slash
      strcpy_s(&pPath[length], nMaxLen - length, pack_file->m_ZipName.String());
      V_AppendSlash(&pPath[length], nMaxLen - length);
      length += strlen(pack_file->m_ZipName.String()) + 1;
    }
  }

  // Return 1 extra for the nullptr terminator
  return length + 1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pPath -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::RemoveSearchPath(const char *pPath, const char *pathID) {
  AsyncFinishAll();

  char newPath[MAX_FILEPATH];
  newPath[0] = 0;

  if (pPath) {
    // +2 for '\0' and potential slash added at end.
    strcpy_s(newPath, pPath);
#ifdef _WIN32  // don't do this on linux!
    Q_strlower(newPath);
#endif

    if (V_stristr(newPath, ".bsp")) {
      Q_FixSlashes(newPath);
    } else {
      AddSeperatorAndFixPath(newPath);
    }
  }

  CUtlSymbol lookup = g_PathIDTable.AddString(newPath);
  CUtlSymbol id = g_PathIDTable.AddString(pathID);

  bool bret = false;

  // Count backward since we're possibly deleting one or more pack files, too
  int i;
  int c = m_SearchPaths.Count();
  for (i = c - 1; i >= 0; i--) {
    if (newPath[0] && m_SearchPaths[i].GetPath() != lookup) continue;

    if (FilterByPathID(&m_SearchPaths[i], id)) continue;

    m_SearchPaths.Remove(i);
    bret = true;
  }
  return bret;
}

//-----------------------------------------------------------------------------
// Purpose: Removes all search paths for a given pathID, such as all "GAME"
// paths. Input  : pathID -
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveSearchPaths(const char *pathID) {
  AsyncFinishAll();

  int nCount = m_SearchPaths.Count();
  for (int i = nCount - 1; i >= 0; i--) {
    if (!_stricmp(m_SearchPaths.Element(i).GetPathIDString(), pathID)) {
      m_SearchPaths.FastRemove(i);
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::FindWritePath(
    const char *pFilename, const char *pathID) {
  CUtlSymbol lookup = g_PathIDTable.AddString(pathID);

  AUTO_LOCK(m_SearchPathsMutex);

  // a pathID has been specified, find the first match in the path list
  int c = m_SearchPaths.Count();
  for (int i = 0; i < c; i++) {
    // pak files are not allowed to be written to...
    CSearchPath *pSearchPath = &m_SearchPaths[i];
    if (pSearchPath->GetPackFile()) {
      continue;
    }

    if (!pathID || (pSearchPath->GetPathID() == lookup)) {
      return pSearchPath;
    }
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Finds a search path that should be used for writing to, given a
// pathID.
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::GetWritePath(const char *pFilename,
                                          const char *pathID) {
  CSearchPath *pSearchPath = nullptr;
  if (pathID && pathID[0] != '\0') {
    pSearchPath = FindWritePath(pFilename, pathID);
    if (pSearchPath) {
      return pSearchPath->GetPathString();
    }

    Warning(FILESYSTEM_WARNING, "Requested non-existent write path %s!\n",
            pathID);
  }

  pSearchPath = FindWritePath(pFilename, "DEFAULT_WRITE_PATH");
  if (pSearchPath) {
    return pSearchPath->GetPathString();
  }

  pSearchPath = FindWritePath(
      pFilename, nullptr);  // okay, just return the first search path added!
  if (pSearchPath) {
    return pSearchPath->GetPathString();
  }

  // Hope this is reasonable!!
  return ".\\";
}

//-----------------------------------------------------------------------------
// Reads/writes files to utlbuffers.  Attempts alignment fixups for optimal read
//-----------------------------------------------------------------------------
CThreadLocal<char *> g_pszReadFilename;
bool CBaseFileSystem::ReadToBuffer(FileHandle_t fp, CUtlBuffer &buf,
                                   int nMaxBytes, FSAllocFunc_t pfnAlloc) {
  SetBufferSize(fp, 0);  // TODO: what if it's a pack file? restore buffer size?

  int nBytesToRead = Size(fp);
  if (nBytesToRead == 0) {
    // no data in file
    return true;
  }

  if (nMaxBytes > 0) {
    // can't read more than file has
    nBytesToRead = std::min(nMaxBytes, nBytesToRead);
  }

  int nBytesRead = 0;
  int nBytesOffset = 0;

  int iStartPos = Tell(fp);

  if (nBytesToRead != 0) {
    int nBytesDestBuffer = nBytesToRead;
    unsigned nSizeAlign = 0, nBufferAlign = 0, nOffsetAlign = 0;

    bool bBinary = !(buf.IsText() && !buf.ContainsCRLF());

    if (bBinary && !IsLinux() && !buf.IsExternallyAllocated() && !pfnAlloc &&
        (buf.TellPut() == 0) && (buf.TellGet() == 0) && (iStartPos % 4 == 0) &&
        GetOptimalIOConstraints(fp, &nOffsetAlign, &nSizeAlign,
                                &nBufferAlign)) {
      // correct conditions to allow an optimal read
      if (iStartPos % nOffsetAlign != 0) {
        // move starting position back to nearest alignment
        nBytesOffset = (iStartPos % nOffsetAlign);
        Assert((iStartPos - nBytesOffset) % nOffsetAlign == 0);
        Seek(fp, -nBytesOffset, FILESYSTEM_SEEK_CURRENT);

        // going to read from aligned start, increase target buffer size by
        // offset alignment
        nBytesDestBuffer += nBytesOffset;
      }

      // snap target buffer size to its size alignment
      // add additional alignment slop for target pointer adjustment
      nBytesDestBuffer =
          AlignValue(nBytesDestBuffer, nSizeAlign) + nBufferAlign;
    }

    if (!pfnAlloc) {
      buf.EnsureCapacity(nBytesDestBuffer + buf.TellPut());
    } else {
      // caller provided allocator
      void *pMemory = (*pfnAlloc)(g_pszReadFilename.Get(), nBytesDestBuffer);
      buf.SetExternalBuffer(pMemory, nBytesDestBuffer, 0,
                            buf.GetFlags() & ~CUtlBuffer::EXTERNAL_GROWABLE);
    }

    int seekGet = -1;
    if (nBytesDestBuffer != nBytesToRead) {
      // doing optimal read, align target pointer
      int nAlignedBase =
          AlignValue((byte *)buf.Base(), nBufferAlign) - (byte *)buf.Base();
      buf.SeekPut(CUtlBuffer::SEEK_HEAD, nAlignedBase);

      // the buffer read position is slid forward to ignore the addtional
      // starting offset alignment
      seekGet = nAlignedBase + nBytesOffset;
    }

    nBytesRead = ReadEx(buf.PeekPut(), nBytesDestBuffer - nBufferAlign,
                        nBytesToRead + nBytesOffset, fp);
    buf.SeekPut(CUtlBuffer::SEEK_CURRENT, nBytesRead);

    if (seekGet != -1) {
      // can only seek the get after data has been put, otherwise buffer sets
      // overflow error
      buf.SeekGet(CUtlBuffer::SEEK_HEAD, seekGet);
    }

    Seek(fp, iStartPos + (nBytesRead - nBytesOffset), FILESYSTEM_SEEK_HEAD);
  }

  return (nBytesRead != 0);
}

//-----------------------------------------------------------------------------
// Reads/writes files to utlbuffers
// NOTE NOTE!!
// If you change this implementation, copy it into CBaseVMPIFileSystem::ReadFile
// in vmpi_filesystem.cpp
//-----------------------------------------------------------------------------
bool CBaseFileSystem::ReadFile(const char *pFileName, const char *pPath,
                               CUtlBuffer &buf, int nMaxBytes,
                               int nStartingByte, FSAllocFunc_t pfnAlloc) {
  CHECK_DOUBLE_SLASHES(pFileName);

  bool bBinary = !(buf.IsText() && !buf.ContainsCRLF());

  FileHandle_t fp = Open(pFileName, (bBinary) ? "rb" : "rt", pPath);
  if (!fp) return false;

  if (nStartingByte != 0) {
    Seek(fp, nStartingByte, FILESYSTEM_SEEK_HEAD);
  }

  if (pfnAlloc) {
    g_pszReadFilename.Set((char *)pFileName);
  }

  bool bSuccess = ReadToBuffer(fp, buf, nMaxBytes, pfnAlloc);

  Close(fp);

  return bSuccess;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CBaseFileSystem::ReadFileEx(const char *pFileName, const char *pPath,
                                void **ppBuf, bool bNullTerminate,
                                bool bOptimalAlloc, int nMaxBytes,
                                int nStartingByte, FSAllocFunc_t pfnAlloc) {
  FileHandle_t fp = Open(pFileName, "rb", pPath);
  if (!fp) {
    return 0;
  }

  SetBufferSize(fp, 0);  // TODO: what if it's a pack file? restore buffer size?

  int nBytesToRead = Size(fp);
  int nBytesRead = 0;
  if (nMaxBytes > 0) {
    nBytesToRead = std::min(nMaxBytes, nBytesToRead);
    if (bNullTerminate) {
      nBytesToRead--;
    }
  }

  if (nBytesToRead != 0) {
    int nBytesBuf;
    if (!*ppBuf) {
      nBytesBuf = nBytesToRead + ((bNullTerminate) ? 1 : 0);

      if (!pfnAlloc && !bOptimalAlloc) {
        *ppBuf = new byte[nBytesBuf];
      } else if (!pfnAlloc) {
        *ppBuf = AllocOptimalReadBuffer(fp, nBytesBuf, 0);
        nBytesBuf = GetOptimalReadSize(fp, nBytesBuf);
      } else {
        *ppBuf = (*pfnAlloc)(pFileName, nBytesBuf);
      }
    } else {
      nBytesBuf = nMaxBytes;
    }

    if (nStartingByte != 0) {
      Seek(fp, nStartingByte, FILESYSTEM_SEEK_HEAD);
    }

    nBytesRead = ReadEx(*ppBuf, nBytesBuf, nBytesToRead, fp);

    if (bNullTerminate) {
      ((byte *)(*ppBuf))[nBytesToRead] = 0;
    }
  }

  Close(fp);
  return nBytesRead;
}

//-----------------------------------------------------------------------------
// NOTE NOTE!!
// If you change this implementation, copy it into
// CBaseVMPIFileSystem::WriteFile in vmpi_filesystem.cpp
//-----------------------------------------------------------------------------
bool CBaseFileSystem::WriteFile(const char *pFileName, const char *pPath,
                                CUtlBuffer &buf) {
  CHECK_DOUBLE_SLASHES(pFileName);

  const char *pWriteFlags = "wb";
  if (buf.IsText() && !buf.ContainsCRLF()) {
    pWriteFlags = "wt";
  }

  FileHandle_t fp = Open(pFileName, pWriteFlags, pPath);
  if (!fp) return false;

  int nBytesWritten = Write(buf.Base(), buf.TellPut(), fp);

  Close(fp);
  return (nBytesWritten != 0);
}

bool CBaseFileSystem::UnzipFile(const char *pFileName, const char *pPath,
                                const char *pDestination) {
#ifdef OS_POSIX
  Error(" need to hook up zip for linux");
#else
  IZip *pZip = IZip::CreateZip(nullptr, true);

  HANDLE hZipFile = pZip->ParseFromDisk(pFileName);
  if (!hZipFile) {
    Msg("Bad or missing zip file, failed to open '%s'\n", pFileName);
    return false;
  }

  int iZipIndex = -1;
  int iFileSize;
  char szFileName[SOURCE_MAX_PATH];

  // Create Directories
  CreateDirHierarchy(pDestination, pPath);

  while (1) {
    // Get the next file in the zip
    szFileName[0] = '\0';
    iFileSize = 0;
    iZipIndex = pZip->GetNextFilename(iZipIndex, szFileName, sizeof(szFileName),
                                      iFileSize);

    // If there aren't any more files then break out of this while
    if (iZipIndex == -1) break;

    usize iFileNameLength = strlen(szFileName);
    if (szFileName[iFileNameLength - 1] == '/') {
      // Its a directory, so create it
      szFileName[iFileNameLength - 1] = '\0';

      char szFinalName[SOURCE_MAX_PATH];
      sprintf_s(szFinalName, "%s\\%s", pDestination, szFileName);
      CreateDirHierarchy(szFinalName, pPath);
    }
  }

  // Write Files
  while (1) {
    szFileName[0] = '\0';
    iFileSize = 0;
    iZipIndex = pZip->GetNextFilename(iZipIndex, szFileName, sizeof(szFileName),
                                      iFileSize);

    // If there aren't any more files then break out of this while
    if (iZipIndex == -1) break;

    usize iFileNameLength = strlen(szFileName);
    if (szFileName[iFileNameLength - 1] != '/') {
      // It's not a directory, so write the file
      CUtlBuffer fileBuffer;
      fileBuffer.Purge();

      if (pZip->ReadFileFromZip(hZipFile, szFileName, false, fileBuffer)) {
        char szFinalName[SOURCE_MAX_PATH];
        sprintf_s(szFinalName, "%s\\%s", pDestination, szFileName);

        WriteFile(szFinalName, pPath, fileBuffer);
      }
    }
  }

  ::CloseHandle(hZipFile);

  IZip::ReleaseZip(pZip);
#endif

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllSearchPaths() {
  AUTO_LOCK(m_SearchPathsMutex);
  m_SearchPaths.Purge();
  // m_PackFileHandles.Purge();
}

void CBaseFileSystem::LogFileAccess(const char *pFullFileName) {
  if (!m_pLogFile) return;

  char log_buffer[1024];
#if BSPOUTPUT
  sprintf_s(log_buffer, "%s\n%s\n", pShortFileName, pFullFileName);
  fprintf(m_pLogFile, "%s", log_buffer);  // STEAM OK
#else
  char cwd[MAX_FILEPATH];
  if (!_getcwd(cwd, std::size(cwd))) return;

  strcat_s(cwd, "\\");

  if (_strnicmp(cwd, pFullFileName, strlen(cwd)) == 0) {
    const char *just_file_name = pFullFileName + strlen(cwd);
    char target_path[MAX_FILEPATH];

    strcpy_s(target_path, "%fs_target%\\");
    strcat_s(target_path, just_file_name);
    sprintf_s(log_buffer, "copy \"%s\" \"%s\"\n", pFullFileName, target_path);

    char target_dir[MAX_FILEPATH];
    strcpy_s(target_dir, target_path);
    Q_StripFilename(target_dir);

    fprintf_s(m_pLogFile, "mkdir \"%s\"\n", target_dir);  // STEAM OK
    fprintf_s(m_pLogFile, "%s", log_buffer);              // STEAM OK
  } else {
    Assert(0);
  }
#endif
}

class CFileOpenInfo {
 public:
  CFileOpenInfo(CBaseFileSystem *pFileSystem, const char *pFileName,
                const CBaseFileSystem::CSearchPath *path, const char *pOptions,
                int flags, char **ppszResolvedFilename, bool bTrackCRCs)
      : m_pFileSystem(pFileSystem),
        m_pFileName(pFileName),
        m_pPath(path),
        m_pOptions(pOptions),
        m_Flags(flags),
        m_ppszResolvedFilename(ppszResolvedFilename),
        m_bTrackCRCs(bTrackCRCs) {
    // Multiple threads can access the whitelist simultaneously.
    // That's fine, but make sure it doesn't get freed by another thread.
    if (IsPC()) {
      m_pWhitelist = m_pFileSystem->m_FileWhitelist.AddRef();
    } else {
      m_pWhitelist = nullptr;
    }
    m_pFileHandle = nullptr;
    m_bLoadedFromSteamCache = m_bSteamCacheOnly = false;

    if (m_ppszResolvedFilename) *m_ppszResolvedFilename = nullptr;
  }

  ~CFileOpenInfo() { m_pFileSystem->m_FileWhitelist.ReleaseRef(m_pWhitelist); }

  void SetAbsolutePath(const char *pFormat, ...) {
    va_list marker;
    va_start(marker, pFormat);
    vsprintf_s(m_AbsolutePath, pFormat, marker);
    va_end(marker);

    V_FixSlashes(m_AbsolutePath);
  }

  void SetResolvedFilename(const char *pStr) {
    if (m_ppszResolvedFilename) {
      Assert(!(*m_ppszResolvedFilename));
      *m_ppszResolvedFilename = _strdup(pStr);
    }
  }

  // Handles telling CFileTracker about the file we just opened so it can
  // remember where the file came from, and possibly calculate a CRC if
  // necessary.
  void HandleFileCRCTracking(const char *pRelativeFileName) {
    if (m_pFileSystem->m_WhitelistFileTrackingEnabled != 1 || !m_bTrackCRCs)
      return;

    if (m_pFileHandle) {
      if (m_bLoadedFromSteamCache) {
        m_pFileSystem->m_FileTracker.NoteFileLoadedFromSteam(
            pRelativeFileName, m_pPath->GetPathIDString(), m_bSteamCacheOnly);
      } else {
        // Only bother to calculate a CRC if we have a whitelist. If we get a
        // whitelist, we'll go back and calculate CRCs as necessary.
        CFileHandle *fhCRCorNot = nullptr;
        if (m_Flags & FSOPEN_FORCE_TRACK_CRC)
          fhCRCorNot = m_pFileHandle;
        else if (m_pWhitelist && m_pWhitelist->m_pWantCRCList &&
                 m_pWhitelist->m_pWantCRCList->IsFileInList(pRelativeFileName))
          fhCRCorNot = m_pFileHandle;

        m_pFileSystem->m_FileTracker.NoteFileLoadedFromDisk(
            pRelativeFileName, m_pPath->GetPathIDString(), fhCRCorNot);
      }
    } else if (m_bSteamCacheOnly) {
      // Remember that the file failed to load. We only need to do this in the
      // case where we forced it to ignore files on disk (in which case we'll
      // want it to check the disk next time if sv_pure changed).
      m_pFileSystem->m_FileTracker.NoteFileFailedToLoad(
          pRelativeFileName, m_pPath->GetPathIDString());
    }
  }

  // Decides if the file must come from Steam or if it can be allowed to come
  // off disk.
  void DetermineFileLoadInfoParameters(CFileLoadInfo &fileLoadInfo,
                                       bool bIsAbsolutePath) {
    if (m_bTrackCRCs && m_pWhitelist && m_pWhitelist->m_pAllowFromDiskList &&
        !bIsAbsolutePath) {
      Assert(!V_IsAbsolutePath(m_pFileName));  // (This is what bIsAbsolutePath
                                               // is supposed to tell us..)
      // Ask the whitelist if this file must come from Steam.
      fileLoadInfo.m_bSteamCacheOnly =
          !m_pWhitelist->m_pAllowFromDiskList->IsFileInList(m_pFileName);
    } else {
      fileLoadInfo.m_bSteamCacheOnly = false;
    }
  }

 public:
  CBaseFileSystem *m_pFileSystem;
  CWhitelistSpecs *m_pWhitelist;

  // These are output parameters.
  CFileHandle *m_pFileHandle;
  char **m_ppszResolvedFilename;

  const char *m_pFileName;
  const CBaseFileSystem::CSearchPath *m_pPath;
  const char *m_pOptions;
  int m_Flags;
  bool m_bTrackCRCs;

  // Stats about how the file was opened and how we asked the stdio/steam
  // filesystem to open it. Used to decide whether or not we need to generate
  // and store CRCs.
  bool m_bLoadedFromSteamCache;  // Did it get loaded out of the Steam cache?
  bool m_bSteamCacheOnly;  // Are we asking that this file only come from Steam?

  char m_AbsolutePath[MAX_FILEPATH];  // This is set
};

bool CBaseFileSystem::HandleOpenFromZipFile(CFileOpenInfo &openInfo) {
  // an absolute path can encode a zip pack file (i.e. caller wants to open the
  // file from within the pack file) format must strictly be ????.zip\????
  // assuming a reasonable restriction that the zip must be a pre-existing
  // search path zip
  char *pZipExt = V_stristr(openInfo.m_AbsolutePath, ".zip");
  if (!pZipExt) {
    pZipExt = V_stristr(openInfo.m_AbsolutePath, ".bsp");
  }

  if (pZipExt && pZipExt[4] == CORRECT_PATH_SEPARATOR && pZipExt[5]) {
    // want full path to zip only, terminate at slash
    pZipExt[4] = '\0';
    // want relative portion only, everything after the slash
    char *pRelativeFileName = pZipExt + 5;

    // find the zip
    for (int i = 0; i < m_ZipFiles.Count(); i++) {
      CPackFile *pPackFile = m_ZipFiles[i];

      if (_stricmp(pPackFile->m_ZipName.Get(), openInfo.m_AbsolutePath) == 0) {
        openInfo.m_pFileHandle =
            pPackFile->OpenFile(pRelativeFileName, openInfo.m_pOptions);
        if (!pPackFile->m_bIsMapPath)
          openInfo.HandleFileCRCTracking(pRelativeFileName);

        break;
      }
    }

    if (openInfo.m_pFileHandle)
      openInfo.SetResolvedFilename(openInfo.m_pFileName);

    return true;
  } else {
    return false;
  }
}

void CBaseFileSystem::HandleOpenFromPackFile(CPackFile *pPackFile,
                                             CFileOpenInfo &openInfo) {
  openInfo.m_pFileHandle =
      pPackFile->OpenFile(openInfo.m_pFileName, openInfo.m_pOptions);
  if (openInfo.m_pFileHandle) {
    char tempStr[SOURCE_MAX_PATH * 2 + 2];
    sprintf_s(tempStr, "%s%c%s", pPackFile->m_ZipName.String(),
              CORRECT_PATH_SEPARATOR, openInfo.m_pFileName);
    openInfo.SetResolvedFilename(tempStr);
  }

  // If it's a BSP file, then the BSP file got CRC'd elsewhere so no need to
  // verify stuff in there.
  if (!pPackFile->m_bIsMapPath)
    openInfo.HandleFileCRCTracking(openInfo.m_pFileName);
}

void CBaseFileSystem::HandleOpenRegularFile(CFileOpenInfo &openInfo,
                                            bool bIsAbsolutePath) {
  // Setup the parameters for the call (like to tell Steam to force the file to
  // come out of the Steam caches or not).
  CFileLoadInfo fileLoadInfo;
  openInfo.DetermineFileLoadInfoParameters(fileLoadInfo, bIsAbsolutePath);

  int64_t size;
  FILE *fp = Trace_FOpen(openInfo.m_AbsolutePath, openInfo.m_pOptions,
                         openInfo.m_Flags, &size, &fileLoadInfo);
  if (fp) {
    if (m_pLogFile) {
      LogFileAccess(openInfo.m_AbsolutePath);
    }

    if (m_bOutputDebugString) {
#ifdef _WIN32
      Plat_DebugString("fs_debug: ");
      Plat_DebugString(openInfo.m_AbsolutePath);
      Plat_DebugString("\n");
#elif OS_POSIX
      fprintf(stderr, "fs_debug: %s\n", openInfo.m_AbsolutePath);
#endif
    }

    openInfo.m_pFileHandle = new CFileHandle(this);
    openInfo.m_pFileHandle->m_pFile = fp;
    openInfo.m_pFileHandle->m_type = FT_NORMAL;
    openInfo.m_pFileHandle->m_nLength = size;

    openInfo.SetResolvedFilename(openInfo.m_AbsolutePath);

    // Remember what was returned by the Steam filesystem and track the CRC.
    openInfo.m_bLoadedFromSteamCache = fileLoadInfo.m_bLoadedFromSteamCache;
    openInfo.m_bSteamCacheOnly = fileLoadInfo.m_bSteamCacheOnly;
    if (!bIsAbsolutePath) openInfo.HandleFileCRCTracking(openInfo.m_pFileName);
  }
}

//-----------------------------------------------------------------------------
// Purpose: The base file search goes through here
// Input  : *path -
//			*pFileName -
//			*pOptions -
//			packfile -
//			*filetime -
// Output : FileHandle_t
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::FindFile(const CSearchPath *path,
                                       const char *pFileName,
                                       const char *pOptions, unsigned flags,
                                       char **ppszResolvedFilename,
                                       bool bTrackCRCs) {
  VPROF("CBaseFileSystem::FindFile");

  CFileOpenInfo openInfo(this, pFileName, path, pOptions, flags,
                         ppszResolvedFilename, bTrackCRCs);
  bool bIsAbsolutePath = V_IsAbsolutePath(pFileName);
  if (bIsAbsolutePath) {
    openInfo.SetAbsolutePath("%s", pFileName);

    // Check if it's of the form C:/a/b/c/blah.zip/materials/blah.vtf
    if (HandleOpenFromZipFile(openInfo)) {
      return (FileHandle_t)openInfo.m_pFileHandle;
    }
  } else {
    // Caller provided a relative path
    if (path->GetPackFile()) {
      HandleOpenFromPackFile(path->GetPackFile(), openInfo);
      return (FileHandle_t)openInfo.m_pFileHandle;
    } else {
      openInfo.SetAbsolutePath("%s%s", path->GetPathString(), pFileName);
    }
  }

  // now have an absolute name
  HandleOpenRegularFile(openInfo, bIsAbsolutePath);
  return (FileHandle_t)openInfo.m_pFileHandle;
}

FileHandle_t CBaseFileSystem::FindFileInSearchPaths(
    const char *pFileName, const char *pOptions, const char *pathID,
    unsigned flags, char **ppszResolvedFilename, bool bTrackCRCs) {
  // Run through all the search paths.
  CSearchPathsIterator iter(this, &pFileName, pathID, FILTER_NONE);

  for (auto *search_path = iter.GetFirst(); search_path != nullptr;
       search_path = iter.GetNext()) {
    FileHandle_t filehandle = FindFile(search_path, pFileName, pOptions, flags,
                                       ppszResolvedFilename, bTrackCRCs);
    if (filehandle) return filehandle;
  }

  return (FileHandle_t) nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenForRead(const char *pFileName,
                                          const char *pOptions, unsigned flags,
                                          const char *pathID,
                                          char **ppszResolvedFilename) {
  VPROF("CBaseFileSystem::OpenForRead");
  return FindFileInSearchPaths(pFileName, pOptions, pathID, flags,
                               ppszResolvedFilename, true);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenForWrite(const char *pFileName,
                                           const char *pOptions,
                                           const char *pathID) {
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pFileName, pathID, tempPathID);

  // Opening for write or append uses the write path
  // Unless an absolute path is specified...
  const char *pTmpFileName;
  char szScratchFileName[SOURCE_MAX_PATH];
  if (Q_IsAbsolutePath(pFileName)) {
    pTmpFileName = pFileName;
  } else {
    ComputeFullWritePath(szScratchFileName, sizeof(szScratchFileName),
                         pFileName, pathID);
    pTmpFileName = szScratchFileName;
  }

  int64_t size;
  FILE *fp = Trace_FOpen(pTmpFileName, pOptions, 0, &size);
  if (fp) {
    CFileHandle *fh = new CFileHandle{this};
    fh->m_nLength = size;
    fh->m_type = FT_NORMAL;
    fh->m_pFile = fp;

    return (FileHandle_t)fh;
  }

  return (FileHandle_t) nullptr;
}

// This looks for UNC-type filename specifiers, which should be used instead of
// passing in path ID. So if it finds //mod/cfg/config.cfg, it translates
// pFilename to "cfg/config.cfg" and pPathID to "mod" (mod is placed in
// tempPathID).
void CBaseFileSystem::ParsePathID(const char *&pFilename, const char *&pPathID,
                                  char tempPathID[SOURCE_MAX_PATH]) {
  tempPathID[0] = 0;

  if (!pFilename || pFilename[0] == 0) return;

  // TODO(d.rattman): Pain! Backslashes are used to denote network drives,
  // forward to denote path ids HOORAY! We call FixSlashes everywhere. That will
  // definitely not work I'm not changing it yet though because I don't know how
  // painful the bugs would be that would be generated
  bool bIsForwardSlash = (pFilename[0] == '/' && pFilename[1] == '/');
  //	bool bIsBackwardSlash = ( pFilename[0] == '\\' && pFilename[1] == '\\'
  //);
  if (!bIsForwardSlash)  //&& !bIsBackwardSlash )
    return;

  // They're specifying two path IDs. Ignore the one passed-in.
  if (pPathID) {
    Warning(FILESYSTEM_WARNING, "FS: Specified two path IDs (%s, %s).\n",
            pFilename, pPathID);
  }

  // Parse out the path ID.
  const char *pIn = &pFilename[2];
  char *pOut = tempPathID;
  while (*pIn && !PATHSEPARATOR(*pIn) &&
         (pOut - tempPathID) < (SOURCE_MAX_PATH - 1)) {
    *pOut++ = *pIn++;
  }

  *pOut = 0;

  if (tempPathID[0] == '*') {
    // * means nullptr.
    pPathID = nullptr;
  } else {
    pPathID = tempPathID;
  }

  // Move pFilename up past the part with the path ID.
  if (*pIn == 0)
    pFilename = pIn;
  else
    pFilename = pIn + 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::Open(const char *pFileName, const char *pOptions,
                                   const char *pathID) {
  return OpenEx(pFileName, pOptions, 0, pathID);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenEx(const char *pFileName,
                                     const char *pOptions, unsigned flags,
                                     const char *pathID,
                                     char **ppszResolvedFilename) {
  VPROF_BUDGET("CBaseFileSystem::Open", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  CHECK_DOUBLE_SLASHES(pFileName);

  if (!IsRetail() && ThreadInMainThread() &&
      fs_report_sync_opens.GetInt() > 0) {
    ::Warning("Open( %s )\n", pFileName);
  }

  // Allow for UNC-type syntax to specify the path ID.
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pFileName, pathID, tempPathID);
  char tempFileName[SOURCE_MAX_PATH];
  strcpy_s(tempFileName, pFileName);
  Q_FixSlashes(tempFileName);
#ifdef _WIN32
  Q_strlower(tempFileName);
#endif

  // Try each of the search paths in succession
  // TODO(d.rattman): call createdirhierarchy upon opening for write.
  if (strchr(pOptions, 'r') && !strchr(pOptions, '+')) {
    return OpenForRead(tempFileName, pOptions, flags, pathID,
                       ppszResolvedFilename);
  }

  return OpenForWrite(tempFileName, pOptions, pathID);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::Close(FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::Close", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  if (!file) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Close nullptr file handle!\n");
    return;
  }

  delete (CFileHandle *)file;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::Seek(FileHandle_t file, int pos,
                           FileSystemSeek_t whence) {
  VPROF_BUDGET("CBaseFileSystem::Seek", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING, "Tried to Seek nullptr file handle!\n");
    return;
  }

  fh->Seek(pos, whence);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : file -
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Tell(FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::Tell", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  if (!file) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Tell nullptr file handle!\n");
    return 0;
  }

  // Pack files are relative
  return ((CFileHandle *)file)->Tell();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : file -
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size(FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::Size", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  if (!file) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Size nullptr file handle!\n");
    return 0;
  }

  return ((CFileHandle *)file)->Size();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : file -
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size(const char *pFileName, const char *pPathID) {
  VPROF_BUDGET("CBaseFileSystem::Size", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  CHECK_DOUBLE_SLASHES(pFileName);

  // handle the case where no name passed...
  if (!pFileName || !pFileName[0]) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Size nullptr filename!\n");
    return 0;
  }

  if (IsPC()) {
    // If we have a whitelist and it's forcing the file to load from Steam
    // instead of from disk, then do this the slow way, otherwise we'll get the
    // wrong file size (i.e. the size of the file on disk).
    CWhitelistSpecs *pWhitelist = m_FileWhitelist.AddRef();
    if (pWhitelist) {
      bool bAllowFromDisk =
          pWhitelist->m_pAllowFromDiskList->IsFileInList(pFileName);
      m_FileWhitelist.ReleaseRef(pWhitelist);

      if (!bAllowFromDisk) {
        FileHandle_t fh = Open(pFileName, "rb", pPathID);
        if (fh) {
          unsigned int ret = Size(fh);
          Close(fh);
          return ret;
        } else {
          return 0;
        }
      }
    }
  }

  // Ok, fall through to the fast path.
  int iSize = 0;

  CSearchPathsIterator iter(this, &pFileName, pPathID);
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    iSize = FastFindFile(pSearchPath, pFileName);
    if (iSize > 0) {
      break;
    }
  }
  return iSize;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *path -
//			*pFileName -
// Output : long
//-----------------------------------------------------------------------------
long CBaseFileSystem::FastFileTime(const CSearchPath *path,
                                   const char *pFileName) {
  struct _stat buf;

  if (path->GetPackFile()) {
    int nIndex, nLength;
    int64_t nPosition;

    // If we found the file:
    if (path->GetPackFile()->FindFile(pFileName, nIndex, nPosition, nLength)) {
      return (path->GetPackFile()->m_lPackFileTime);
    }
  } else {
    // Is it an absolute path?
    char pTmpFileName[MAX_FILEPATH];

    if (Q_IsAbsolutePath(pFileName)) {
      strcpy_s(pTmpFileName, pFileName);
    } else {
      sprintf_s(pTmpFileName, "%s%s", path->GetPathString(), pFileName);
    }

    Q_FixSlashes(pTmpFileName);

    if (FS_stat(pTmpFileName, &buf) != -1) {
      return buf.st_mtime;
    }
#ifdef OS_POSIX
    const char *realName = findFileInDirCaseInsensitive(pTmpFileName);
    if (realName && FS_stat(realName, &buf) != -1) {
      free((void *)realName);
      return buf.st_size;
    }
#endif
  }

  return (0L);
}

int CBaseFileSystem::FastFindFile(const CSearchPath *path,
                                  const char *pFileName) {
  struct _stat buf;

  if (path->GetPackFile()) {
    CFileHandle *fh = (CFileHandle *)path->GetPackFile()->OpenFile(pFileName);
    if (fh) {
      int size = fh->Size();
      delete fh;
      return size;
    }
  } else {
    // Is it an absolute path?
    char pTmpFileName[MAX_FILEPATH];

    if (Q_IsAbsolutePath(pFileName)) {
      strcpy_s(pTmpFileName, pFileName);
    } else {
      sprintf_s(pTmpFileName, "%s%s", path->GetPathString(), pFileName);
    }

    Q_FixSlashes(pTmpFileName);

    if (FS_stat(pTmpFileName, &buf) != -1) {
      LogAccessToFile("stat", pTmpFileName, "");

      return buf.st_size;
    }
#ifdef OS_POSIX
    const char *realName = findFileInDirCaseInsensitive(pTmpFileName);
    if (realName && FS_stat(realName, &buf) != -1) {
      return buf.st_size;
    }
#endif
  }

  return (-1);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseFileSystem::EndOfFile(FileHandle_t file) {
  if (!file) {
    Warning(FILESYSTEM_WARNING,
            "FS:  Tried to EndOfFile nullptr file handle!\n");
    return true;
  }

  return ((CFileHandle *)file)->EndOfFile();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseFileSystem::Read(void *pOutput, int size, FileHandle_t file) {
  return ReadEx(pOutput, size, size, file);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseFileSystem::ReadEx(void *pOutput, int destSize, int size,
                            FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::Read", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  if (!file) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Read nullptr file handle!\n");
    return 0;
  }
  if (size < 0) {
    return 0;
  }
  return ((CFileHandle *)file)->Read(pOutput, destSize, size);
}

//-----------------------------------------------------------------------------
// Purpose: Blow away current readers
// Input  :  -
//-----------------------------------------------------------------------------
void CBaseFileSystem::UnloadCompiledKeyValues() {
#ifndef DEDICATED
  for (int i = 0; i < IFileSystem::NUM_PRELOAD_TYPES; ++i) {
    delete m_PreloadData[i].m_pReader;
    m_PreloadData[i].m_pReader = nullptr;
  }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Put data file into list of at specific slot, will be loaded when
// ::SetupPreloadData() gets called Input  : type -
//			*archiveFile -
//-----------------------------------------------------------------------------
void CBaseFileSystem::LoadCompiledKeyValues(KeyValuesPreloadType_t type,
                                            char const *archiveFile) {
  // Just add to list for appropriate loader
  Assert(type >= 0 && type < IFileSystem::NUM_PRELOAD_TYPES);
  CompiledKeyValuesPreloaders_t &loader = m_PreloadData[type];
  Assert(loader.m_CacheFile == 0);
  loader.m_CacheFile = FindOrAddFileName(archiveFile);
}

//-----------------------------------------------------------------------------
// Purpose: Takes a passed in KeyValues& head and fills in the precompiled
// keyvalues data into it. Input  : head -
//			type -
//			*filename -
//			*pPathID -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::LoadKeyValues(KeyValues &head,
                                    KeyValuesPreloadType_t type,
                                    char const *filename,
                                    char const *pPathID /*= 0*/) {
  bool bret = true;

#ifndef DEDICATED
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(filename, pPathID, tempPathID);

  // TODO(d.rattman):  THIS STUFF DOESN'T TRACK pPathID AT ALL RIGHT NOW!!!!!
  if (!m_PreloadData[type].m_pReader ||
      !m_PreloadData[type].m_pReader->InstanceInPlace(head, filename)) {
    bret = head.LoadFromFile(this, filename, pPathID);
  }
  return bret;
#else
  bret = head.LoadFromFile(this, filename, pPathID);
  return bret;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: If the "PreloadedData" hasn't been purged, then this'll try and
// instance the KeyValues using the fast path of
/// compiled keyvalues loaded during startup.
// Otherwise, it'll just fall through to the regular KeyValues loading routines
// Input  : type -
//			*filename -
//			*pPathID -
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *CBaseFileSystem::LoadKeyValues(KeyValuesPreloadType_t type,
                                          char const *filename,
                                          char const *pPathID /*= 0*/) {
  KeyValues *kv = nullptr;

  if (!m_PreloadData[type].m_pReader) {
    kv = new KeyValues(filename);
    if (kv) {
      kv->LoadFromFile(this, filename, pPathID);
    }
  } else {
#ifndef DEDICATED
    // TODO(d.rattman):  THIS STUFF DOESN'T TRACK pPathID AT ALL RIGHT NOW!!!!!
    kv = m_PreloadData[type].m_pReader->Instance(filename);
    if (!kv) {
      kv = new KeyValues(filename);
      if (kv) {
        kv->LoadFromFile(this, filename, pPathID);
      }
    }
#endif
  }
  return kv;
}

//-----------------------------------------------------------------------------
// Purpose: This is the fallback method of reading the name of the first key in
// the file Input  : *filename -
//			*pPathID -
//			*rootName -
//			bufsize -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::LookupKeyValuesRootKeyName(char const *filename,
                                                 char const *pPathID,
                                                 char *rootName,
                                                 size_t bufsize) {
  if (FileExists(filename, pPathID)) {
    // open file and get shader name
    FileHandle_t hFile = Open(filename, "r", pPathID);
    if (hFile == FILESYSTEM_INVALID_HANDLE) return false;

    char buf[128];
    ReadLine(buf, sizeof(buf), hFile);
    Close(hFile);

    // The name will possibly come in as "foo"\n

    // So we need to strip the starting " character
    char *pStart = buf;
    if (*pStart == '\"') {
      ++pStart;
    }
    // Then copy the rest of the string
    strcpy_s(rootName, bufsize, pStart);

    // And then strip off the \n and the " character at the end, in that order
    usize len = strlen(pStart);
    while (len > 0 && rootName[len - 1] == '\n') {
      rootName[len - 1] = '\0';
      --len;
    }
    while (len > 0 && rootName[len - 1] == '\"') {
      rootName[len - 1] = '\0';
      --len;
    }

    return true;
  }

  return false;
}

// Tries to look up the name of the first key in the file from the compiled data
bool CBaseFileSystem::ExtractRootKeyName(KeyValuesPreloadType_t type,
                                         char *outbuf, size_t bufsize,
                                         char const *filename,
                                         char const *pPathID /*= 0*/) {
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(filename, pPathID, tempPathID);

  bool bret = true;

  if (!m_PreloadData[type].m_pReader) {
    // Use fallback
    bret = LookupKeyValuesRootKeyName(filename, pPathID, outbuf, bufsize);
  } else {
#ifndef DEDICATED
    // Try to use cache
    bret = m_PreloadData[type].m_pReader->LookupKeyValuesRootKeyName(
        filename, outbuf, bufsize);
    if (!bret) {
      // Not in cache, use fallback
      bret = LookupKeyValuesRootKeyName(filename, pPathID, outbuf, bufsize);
    }
#endif
  }
  return bret;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetupPreloadData() {
  int i;

  for (i = 0; i < m_SearchPaths.Count(); i++) {
    CPackFile *pf = m_SearchPaths[i].GetPackFile();
    if (pf) {
      pf->SetupPreloadData();
    }
  }

#ifndef DEDICATED
  if (!CommandLine()->FindParm("-fs_nopreloaddata")) {
    // Loads in the precompiled keyvalues data for each type
    for (i = 0; i < NUM_PRELOAD_TYPES; ++i) {
      CompiledKeyValuesPreloaders_t &preloader = m_PreloadData[i];
      Assert(!preloader.m_pReader);

      char fn[SOURCE_MAX_PATH];
      if (preloader.m_CacheFile != 0 &&
          String(preloader.m_CacheFile, fn, sizeof(fn))) {
        preloader.m_pReader = new CCompiledKeyValuesReader();
        preloader.m_pReader->LoadFile(fn);
      }
    }
  }
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::DiscardPreloadData() {
  int i;
  for (i = 0; i < m_SearchPaths.Count(); i++) {
    CPackFile *pf = m_SearchPaths[i].GetPackFile();
    if (pf) {
      pf->DiscardPreloadData();
    }
  }

  UnloadCompiledKeyValues();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseFileSystem::Write(void const *pInput, int size, FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::Write", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  AUTOBLOCKREPORTER_FH(Write, this, true, file, FILESYSTEM_BLOCKING_SYNCHRONOUS,
                       FileBlockingItem::FB_ACCESS_WRITE);

  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Write nullptr file handle!\n");
    return 0;
  }
  return fh->Write(pInput, size);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseFileSystem::FPrintf(FileHandle_t file, const char *pFormat, ...) {
  va_list args;
  va_start(args, pFormat);

  VPROF_BUDGET("CBaseFileSystem::FPrintf", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to FPrintf nullptr file handle!\n");
    return 0;
  }

  char buffer[65535];
  int len = vsprintf_s(buffer, pFormat, args);
  len = fh->Write(buffer, len);
  va_end(args);

  return len;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetBufferSize(FileHandle_t file, unsigned nBytes) {
  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING,
            "FS:  Tried to SetBufferSize nullptr file handle!\n");
    return;
  }
  fh->SetBufferSize(nBytes);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsOk(FileHandle_t file) {
  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to IsOk nullptr file handle!\n");
    return false;
  }

  return fh->IsOK();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::Flush(FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::Flush", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING, "FS:  Tried to Flush nullptr file handle!\n");
    return;
  }

  fh->Flush();
}

bool CBaseFileSystem::Precache(const char *pFileName, const char *pPathID) {
  CHECK_DOUBLE_SLASHES(pFileName);

  // Allow for UNC-type syntax to specify the path ID.
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pFileName, pPathID, tempPathID);
  Assert(pPathID);

  // Really simple, just open, the file, read it all in and close it.
  // We probably want to use file mapping to do this eventually.
  FileHandle_t f = Open(pFileName, "rb", pPathID);
  if (!f) return false;

  // not for consoles, the read discard is a negative benefit, slow and clobbers
  // small drive caches
  if (IsPC()) {
    char buffer[16384];
    while (sizeof(buffer) == Read(buffer, sizeof(buffer), f))
      ;
  }

  Close(f);

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
char *CBaseFileSystem::ReadLine(char *pOutput, int maxChars,
                                FileHandle_t file) {
  VPROF_BUDGET("CBaseFileSystem::ReadLine", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  CFileHandle *fh = (CFileHandle *)file;
  if (!fh) {
    Warning(FILESYSTEM_WARNING,
            "FS:  Tried to ReadLine nullptr file handle!\n");
    return nullptr;
  }
  ++m_Stats.nReads;

  int nRead = 0;

  // Read up to maxchars:
  while (nRead < (maxChars - 1)) {
    // Are we at the end of the file?
    if (1 != fh->Read(pOutput + nRead, 1)) break;

    // Translate for text mode files:
    if (fh->m_type == FT_PACK_TEXT && pOutput[nRead] == '\r') {
      // Ignore \r
      continue;
    }

    // We're done when we hit a '\n'
    if (pOutput[nRead] == '\n') {
      nRead++;
      break;
    }

    // Get outta here if we find a nullptr.
    if (pOutput[nRead] == '\0') {
      pOutput[nRead] = '\n';
      nRead++;
      break;
    }

    nRead++;
  }

  if (nRead < maxChars) pOutput[nRead] = '\0';

  m_Stats.nBytesRead += nRead;
  return (nRead) ? pOutput : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pFileName -
// Output : long
//-----------------------------------------------------------------------------
long CBaseFileSystem::GetFileTime(const char *pFileName, const char *pPathID) {
  VPROF_BUDGET("CBaseFileSystem::GetFileTime",
               VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  CHECK_DOUBLE_SLASHES(pFileName);

  CSearchPathsIterator iter(this, &pFileName, pPathID);

  char tempFileName[SOURCE_MAX_PATH];
  strcpy_s(tempFileName, pFileName);
  Q_FixSlashes(tempFileName);
#ifdef _WIN32
  Q_strlower(tempFileName);
#endif

  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    long ft = FastFileTime(pSearchPath, tempFileName);
    if (ft != 0L) {
      if (!pSearchPath->GetPackFile() && m_LogFuncs.Count()) {
        char pTmpFileName[MAX_FILEPATH];
        if (strchr(tempFileName, ':')) {
          strcpy_s(pTmpFileName, tempFileName);
        } else {
          sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(),
                    tempFileName);
        }

        Q_FixSlashes(tempFileName);

        LogAccessToFile("filetime", pTmpFileName, "");
      }

      return ft;
    }
  }
  return 0L;
}

long CBaseFileSystem::GetPathTime(const char *pFileName, const char *pPathID) {
  VPROF_BUDGET("CBaseFileSystem::GetFileTime",
               VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  CSearchPathsIterator iter(this, &pFileName, pPathID);

  char tempFileName[SOURCE_MAX_PATH];
  strcpy_s(tempFileName, pFileName);
  Q_FixSlashes(tempFileName);
#ifdef _WIN32
  Q_strlower(tempFileName);
#endif

  long pathTime = 0L;
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    long ft = FastFileTime(pSearchPath, tempFileName);
    if (ft > pathTime) pathTime = ft;
    if (ft != 0L) {
      if (!pSearchPath->GetPackFile() && m_LogFuncs.Count()) {
        char pTmpFileName[MAX_FILEPATH];
        if (strchr(tempFileName, ':')) {
          strcpy_s(pTmpFileName, tempFileName);
        } else {
          sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(),
                    tempFileName);
        }

        Q_FixSlashes(tempFileName);

        LogAccessToFile("filetime", pTmpFileName, "");
      }
    }
  }
  return pathTime;
}

bool CBaseFileSystem::ShouldGameReloadFile(const char *pFilename) {
  if (V_IsAbsolutePath(pFilename)) {
    if (m_WhitelistSpewFlags & WHITELIST_SPEW_RELOAD_FILES) {
      Msg("Whitelist -       reload (absolute path) %s\n", pFilename);
    }

    // They should be checking with relative filenames, but this is easy to
    // remedy if we need to. Easy enough to remedy if we want.. just strip off
    // the path ID prefixes until we find the file.
    Assert(false);
    return true;
  }

  FileInfo *fileInfos[256];
  int nFileInfos = m_FileTracker.GetFileInfos(
      fileInfos, SOURCE_ARRAYSIZE(fileInfos), pFilename);
  if (nFileInfos == 0) {
    // Ain't heard of this file. It probably came from a BSP or a pak file.
    if (m_WhitelistSpewFlags & WHITELIST_SPEW_DONT_RELOAD_FILES) {
      Msg("Whitelist - don't reload (unheard-of-file) %s\n", pFilename);
    }
    return false;
  }

  // Note: This might be 0, in which case all files are allowed to come off
  // disk.
  bool bFileAllowedToComeFromDisk = true;
  CWhitelistSpecs *pWhitelist = m_FileWhitelist.GetInMainThread();
  if (pWhitelist)
    bFileAllowedToComeFromDisk =
        pWhitelist->m_pAllowFromDiskList->IsFileInList(pFilename);

  // Since we don't require the game to specify which path ID it's interested in
  // here, there's a small amount of ambiguity here (because 2 files with the
  // same name could have been loaded from different path IDs). This case should
  // be extremely rare, and we error on the side of simplicity (don't require
  // the game to remember which path ID its files were opened from).
  //
  // In the case where there are multiple files, we error on the side of
  // reloading the file - if any of the files here would require a reload, then
  // we tell the game to reload this file even if it might not have strictly
  // needed to reload it.
  bool bRet = false;
  for (int i = 0; i < nFileInfos; i++) {
    FileInfo *pFileInfo = fileInfos[i];

    // See comments above k_eFileFlagsFailedToLoadLastTime for info about this
    // case.
    if (bFileAllowedToComeFromDisk &&
        (pFileInfo->m_Flags & k_eFileFlagsFailedToLoadLastTime)) {
      bRet = true;
      break;
    }

    if (pFileInfo->m_Flags & k_eFileFlagsLoadedFromSteam) {
      if ((pFileInfo->m_Flags & k_eFileFlagsForcedLoadFromSteam) &&
          bFileAllowedToComeFromDisk) {
        // So.. the last time we loaded this file, we forced it to come from
        // Steam, but the new whitelist says it's ok if this file is loaded off
        // disk. So reload it.
        //
        // TODO: we could optimize this by checking if there even IS a file on
        // disk that would override the Steam one,
        //       and in that case, don't tell the game to reload it.
        //
        //       We could also optimize it if we remembered whether we told
        //       Steam to allow loads off disk or not.
        //		 If we did allow loads and Steam still got the file from
        // Steam, then we know that there isn't
        //		 a file on disk here, and therefore we wouldn't have to
        // reload it now.
        bRet = true;
        break;
      } else {
        // So we loaded the file from Steam last time, and the new whitelist
        // only allows it to come from Steam, so we're ok.
        // return false;
      }
    } else {
      if (bFileAllowedToComeFromDisk) {
        // No need to reload. The new whitelist says this file can come off
        // disk, and the last time we loaded it, it was off disk. The client
        // will still verify the CRC of the file with the server to make sure
        // its file is legit..
        // return false;
      } else {
        // The file was loaded off disk but the server won't allow that now.
        bRet = true;
        break;
      }
    }
  }

  if ((m_WhitelistSpewFlags & WHITELIST_SPEW_RELOAD_FILES) && bRet)
    Msg("Whitelist -       reload %s\n", pFilename);

  if ((m_WhitelistSpewFlags & WHITELIST_SPEW_DONT_RELOAD_FILES) && !bRet)
    Msg("Whitelist - don't reload %s\n", pFilename);

  return bRet;
}

void CBaseFileSystem::MarkAllCRCsUnverified() {
  m_FileTracker.MarkAllCRCsUnverified();
}

void CBaseFileSystem::CacheFileCRCs(const char *pPathname, ECacheCRCType eType,
                                    IFileList *pFilter) {
  // Get a list of the unique search path names (mod, game, platform, etc).
  CUtlDict<int, int> searchPathNames;
  m_SearchPathsMutex.Lock();
  for (int i = 0; i < m_SearchPaths.Count(); i++) {
    CSearchPath *pSearchPath = &m_SearchPaths[i];
    if (searchPathNames.Find(pSearchPath->GetPathIDString()) ==
        searchPathNames.InvalidIndex())
      searchPathNames.Insert(pSearchPath->GetPathIDString());
  }
  m_SearchPathsMutex.Unlock();

  CacheFileCRCs_R(pPathname, eType, pFilter, searchPathNames);
}

void CBaseFileSystem::CacheFileCRCs_R(const char *pPathname,
                                      ECacheCRCType eType, IFileList *pFilter,
                                      CUtlDict<int, int> &searchPathNames) {
  char searchStr[SOURCE_MAX_PATH];
  bool bRecursive = false;

  if (eType == k_eCacheCRCType_SingleFile) {
    sprintf_s(searchStr, "%s", pPathname);
  } else if (eType == k_eCacheCRCType_Directory) {
    V_ComposeFileName(pPathname, "*.*", searchStr, sizeof(searchStr));
  } else if (eType == k_eCacheCRCType_Directory_Recursive) {
    V_ComposeFileName(pPathname, "*.*", searchStr, sizeof(searchStr));
    bRecursive = true;
  }

  // Get the path we're searching in.
  char pathDirectory[SOURCE_MAX_PATH];
  strcpy_s(pathDirectory, searchStr);
  V_StripLastDir(pathDirectory, sizeof(pathDirectory));

  /*
  Note:	This is tricky because the client could check different path IDs
  with the same filename and we'd either have the same file or a different file
  depending on these two cases:
    a) they have one file : hl2\blah.txt (in this case, checking the GAME and
  MOD path IDs for blah.txt return the same file CRC)
    b) they have two files: hl2\blah.txt AND hl2mp\blah.txt (in this case,
  checking the GAME and MOD path IDs for blah.txt return different file CRCs)
  */

  CUtlDict<CUtlVector<CStoreIDEntry> *, int> filesByStoreID;  // key=filename,
                                                              // value=list of
                                                              // store IDs this
                                                              // filename was
                                                              // found in
  for (int i = searchPathNames.First(); i != searchPathNames.InvalidIndex();
       i = searchPathNames.Next(i)) {
    // Now find all the files..
    int foundStoreID;
    const char *pPathIDStr = searchPathNames.GetElementName(i);
    FileFindHandle_t findHandle;
    const char *pFilename =
        FindFirstHelper(searchStr, pPathIDStr, &findHandle, &foundStoreID);
    while (pFilename) {
      if (pFilename[0] != '.') {
        char relativeName[SOURCE_MAX_PATH];
        V_ComposeFileName(pathDirectory, pFilename, relativeName,
                          sizeof(relativeName));

        if (FindIsDirectory(findHandle)) {
          if (bRecursive)
            CacheFileCRCs_R(relativeName, eType, pFilter, searchPathNames);
        } else {
          if (pFilter->IsFileInList(relativeName)) {
            CStoreIDEntry *pPrevRecord = FindPrevFileByStoreID(
                filesByStoreID, pFilename, pPathIDStr, foundStoreID);
            if (pPrevRecord) {
              // Ok, we already found this file in an earlier search path with
              // the same storeID (i.e. the exact same disk path) so rather than
              // recalculate the CRC redundantly, just copy the CRC from the
              // previous one into a record with the new path ID string. This
              // saves a lot of redundant CRC calculations since logdir,
              // default_write_path, game, and mod all share directories.
              m_FileTracker.CacheFileCRC_Copy(
                  pPathIDStr, relativeName,
                  pPrevRecord->m_PathIDString.String());
            } else {
              // Ok, we want the CRC for this file.
              m_FileTracker.CacheFileCRC(pPathIDStr, relativeName);
            }
          }
        }
      }

      FindData_t *pFindData = &m_FindData[findHandle];
      if (!FindNextFileHelper(pFindData, &foundStoreID)) break;

      pFilename = pFindData->findData.cFileName;
    }
    FindClose(findHandle);
  }
  filesByStoreID.PurgeAndDeleteElements();
}

EFileCRCStatus CBaseFileSystem::CheckCachedFileCRC(
    const char *pPathID, const char *pRelativeFilename, CRC32_t *pCRC) {
  return m_FileTracker.CheckCachedFileCRC(pPathID, pRelativeFilename, pCRC);
}

void CBaseFileSystem::EnableWhitelistFileTracking(bool bEnable) {
  if (m_WhitelistFileTrackingEnabled != -1) {
    Error(
        "CBaseFileSystem::EnableWhitelistFileTracking called more than once.");
  }

  m_WhitelistFileTrackingEnabled = bEnable;
}

void CBaseFileSystem::RegisterFileWhitelist(IFileList *pWantCRCList,
                                            IFileList *pAllowFromDiskList,
                                            IFileList **pFilesToReload) {
  // We can assume the game is going to want CRCs for any files that came off
  // disk after this, so calculate the CRCs for any files we didn't calculate it
  // for before.
  if (pWantCRCList) m_FileTracker.CalculateMissingCRCs(pWantCRCList);

  CWhitelistSpecs *pOldList = m_FileWhitelist.GetInMainThread();
  if (pOldList) {
    m_FileWhitelist.ReleaseRef(
        pOldList);  // Get rid of our reference to it so it can be freed.
    m_FileWhitelist.ResetWhenNoRemainingReferences(
        nullptr);  // Wait for everyone else to stop hanging onto it, then free
                   // it.

    // Free the old ones (other threads shouldn't have access to these anymore
    // because
    pOldList->m_pAllowFromDiskList->Release();
    pOldList->m_pWantCRCList->Release();
  }

  if (pAllowFromDiskList) {
    CWhitelistSpecs *pNewList = new CWhitelistSpecs;
    pNewList->m_pAllowFromDiskList = pAllowFromDiskList;
    pNewList->m_pWantCRCList = pWantCRCList;
    m_FileWhitelist.Init(pNewList);
  }

  // Even if they passed nullptr for their lists, we still want to let them ask
  // which files to reload since the default w/o a whitelist is that all files
  // can come from disk (so if their old whitelist made it force certain files
  // to come from Steam, the game will be told to reload the files in order to
  // give them a chance to get them off disk).
  if (pFilesToReload) {
    // Give back an object that tells them which files they should reload.
    *pFilesToReload = new CFileSystemReloadFileList(this);
  }
}

int CBaseFileSystem::GetUnverifiedCRCFiles(CUnverifiedCRCFile *pFiles,
                                           int nMaxFiles) {
  return m_FileTracker.GetUnverifiedCRCFiles(pFiles, nMaxFiles);
}

int CBaseFileSystem::GetWhitelistSpewFlags() { return m_WhitelistSpewFlags; }

void CBaseFileSystem::SetWhitelistSpewFlags(int flags) {
  m_WhitelistSpewFlags = flags;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pString -
//			maxCharsIncludingTerminator -
//			fileTime -
//-----------------------------------------------------------------------------
void CBaseFileSystem::FileTimeToString(char *pString,
                                       size_t maxCharsIncludingTerminator,
                                       long file_time) {
  time_t the_time = file_time;
  char time_string[32];

  if (!ctime_s(time_string, SOURCE_ARRAYSIZE(time_string), &the_time)) {
    strcpy_s(pString, maxCharsIncludingTerminator, time_string);
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pFileName -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FileExists(const char *pFileName, const char *pPathID) {
  VPROF_BUDGET("CBaseFileSystem::FileExists",
               VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

  CHECK_DOUBLE_SLASHES(pFileName);

  CSearchPathsIterator iter(this, &pFileName, pPathID);
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    int size = FastFindFile(pSearchPath, pFileName);
    if (size >= 0) {
      return true;
    }
  }
  return false;
}

bool CBaseFileSystem::IsFileWritable(char const *pFileName,
                                     char const *pPathID /*=0*/) {
  CHECK_DOUBLE_SLASHES(pFileName);

  struct _stat buf;

  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pFileName, pPathID, tempPathID);

  if (Q_IsAbsolutePath(pFileName)) {
    if (FS_stat(pFileName, &buf) != -1) {
#ifdef WIN32
      if (buf.st_mode & _S_IWRITE)
#elif LINUX
      if (buf.st_mode & S_IWRITE)
#else
      if (buf.st_mode & S_IWRITE)
#endif
      {
        return true;
      }
    }
    return false;
  }

  CSearchPathsIterator iter(this, &pFileName, pPathID, FILTER_CULLPACK);
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    char pTmpFileName[MAX_FILEPATH];
    sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName);
    Q_FixSlashes(pTmpFileName);
    if (FS_stat(pTmpFileName, &buf) != -1) {
#ifdef WIN32
      if (buf.st_mode & _S_IWRITE)
#elif LINUX
      if (buf.st_mode & S_IWRITE)
#else
      if (buf.st_mode & S_IWRITE)
#endif
      {
        return true;
      }
    }
  }
  return false;
}

bool CBaseFileSystem::SetFileWritable(char const *pFileName, bool writable,
                                      const char *pPathID /*= 0*/) {
  CHECK_DOUBLE_SLASHES(pFileName);

#ifdef _WIN32
  int pmode = writable ? (_S_IWRITE | _S_IREAD) : (_S_IREAD);
#else
  int pmode = writable ? (S_IWRITE | S_IREAD) : (S_IREAD);
#endif

  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pFileName, pPathID, tempPathID);

  if (Q_IsAbsolutePath(pFileName)) {
    return (FS_chmod(pFileName, pmode) == 0);
  }

  CSearchPathsIterator iter(this, &pFileName, pPathID, FILTER_CULLPACK);
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    char pTmpFileName[MAX_FILEPATH];
    sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName);
    Q_FixSlashes(pTmpFileName);

    if (FS_chmod(pTmpFileName, pmode) == 0) {
      return true;
    }
  }

  // Failure
  return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pFileName -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsDirectory(const char *pFileName, const char *pathID) {
  CHECK_DOUBLE_SLASHES(pFileName);

  // Allow for UNC-type syntax to specify the path ID.
  struct _stat buf;

  char pTempBuf[SOURCE_MAX_PATH];
  strcpy_s(pTempBuf, pFileName);
  Q_StripTrailingSlash(pTempBuf);
  pFileName = pTempBuf;

  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pFileName, pathID, tempPathID);
  if (Q_IsAbsolutePath(pFileName)) {
    if (FS_stat(pFileName, &buf) != -1) {
      if (buf.st_mode & _S_IFDIR) return true;
    }
    return false;
  }

  CSearchPathsIterator iter(this, &pFileName, pathID, FILTER_CULLPACK);
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    char pTmpFileName[MAX_FILEPATH];
    sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName);
    Q_FixSlashes(pTmpFileName);
    if (FS_stat(pTmpFileName, &buf) != -1) {
      if (buf.st_mode & _S_IFDIR) return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *path -
//-----------------------------------------------------------------------------
void CBaseFileSystem::CreateDirHierarchy(const char *pRelativePath,
                                         const char *pathID) {
  CHECK_DOUBLE_SLASHES(pRelativePath);

  // Allow for UNC-type syntax to specify the path ID.
  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(pRelativePath, pathID, tempPathID);

  char szScratchFileName[SOURCE_MAX_PATH];
  if (!Q_IsAbsolutePath(pRelativePath)) {
    Assert(pathID);
    ComputeFullWritePath(szScratchFileName, sizeof(szScratchFileName),
                         pRelativePath, pathID);
  } else {
    strcpy_s(szScratchFileName, pRelativePath);
  }

  int len = strlen(szScratchFileName) + 1;
  char *end = szScratchFileName + len;
  char *s = szScratchFileName;
  while (s < end) {
    if (*s == CORRECT_PATH_SEPARATOR && s != szScratchFileName &&
        (IsLinux() || *(s - 1) != ':')) {
      *s = '\0';
#if defined(_WIN32)
      _mkdir(szScratchFileName);
#elif defined(OS_POSIX)
      mkdir(szScratchFileName,
            S_IRWXU | S_IRGRP | S_IROTH);  // owner has rwx, rest have r
#endif
      *s = CORRECT_PATH_SEPARATOR;
    }
    s++;
  }

#if defined(_WIN32)
  _mkdir(szScratchFileName);
#elif defined(OS_POSIX)
  mkdir(szScratchFileName, S_IRWXU | S_IRGRP | S_IROTH);
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pWildCard -
//			*pHandle -
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::FindFirstEx(const char *pWildCard,
                                         const char *pPathID,
                                         FileFindHandle_t *pHandle) {
  CHECK_DOUBLE_SLASHES(pWildCard);

  return FindFirstHelper(pWildCard, pPathID, pHandle, nullptr);
}

const char *CBaseFileSystem::FindFirstHelper(const char *pWildCard,
                                             const char *pPathID,
                                             FileFindHandle_t *pHandle,
                                             int *pFoundStoreID) {
  VPROF_BUDGET("CBaseFileSystem::FindFirst",
               VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  Assert(pWildCard);
  Assert(pHandle);

  FileFindHandle_t hTmpHandle = m_FindData.AddToTail();
  FindData_t *pFindData = &m_FindData[hTmpHandle];
  Assert(pFindData);
  if (pPathID) {
    pFindData->m_FilterPathID = g_PathIDTable.AddString(pPathID);
  }
  usize maxlen = strlen(pWildCard) + 1;
  pFindData->wildCardString.AddMultipleToTail(maxlen);
  strcpy_s(pFindData->wildCardString.Base(), maxlen, pWildCard);
  Q_FixSlashes(pFindData->wildCardString.Base());
  pFindData->findHandle = INVALID_HANDLE_VALUE;

  if (Q_IsAbsolutePath(pWildCard)) {
    pFindData->findHandle = FS_FindFirstFile(pWildCard, &pFindData->findData);
    pFindData->currentSearchPathID = -1;
  } else {
    int c = m_SearchPaths.Count();
    for (pFindData->currentSearchPathID = 0; pFindData->currentSearchPathID < c;
         pFindData->currentSearchPathID++) {
      CSearchPath *pSearchPath = &m_SearchPaths[pFindData->currentSearchPathID];

      // TODO(d.rattman):  Should findfirst/next work with pak files?
      if (pSearchPath->GetPackFile()) continue;

      if (FilterByPathID(pSearchPath, pFindData->m_FilterPathID)) continue;

      // already visited this path
      if (pFindData->m_VisitedSearchPaths.MarkVisit(*pSearchPath)) continue;

      char pTmpFileName[MAX_FILEPATH];
      sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(),
                pFindData->wildCardString.Base());
      Q_FixSlashes(pTmpFileName);
      pFindData->findHandle =
          FS_FindFirstFile(pTmpFileName, &pFindData->findData);
      pFindData->m_CurrentStoreID = pSearchPath->m_storeId;

      if (pFindData->findHandle != INVALID_HANDLE_VALUE) break;
    }
  }

  if (pFindData->findHandle != INVALID_HANDLE_VALUE) {
    // Remember that we visited this file already.
    pFindData->m_VisitedFiles.Insert(pFindData->findData.cFileName, 0);

    if (pFoundStoreID) *pFoundStoreID = pFindData->m_CurrentStoreID;

    *pHandle = hTmpHandle;
    return pFindData->findData.cFileName;
  }

  // Handle failure here
  pFindData = 0;
  m_FindData.Remove(hTmpHandle);
  *pHandle = -1;

  return nullptr;
}

const char *CBaseFileSystem::FindFirst(const char *pWildCard,
                                       FileFindHandle_t *pHandle) {
  return FindFirstEx(pWildCard, nullptr, pHandle);
}

// Get the next file, trucking through the path. . don't check for duplicates.
bool CBaseFileSystem::FindNextFileHelper(FindData_t *pFindData,
                                         int *pFoundStoreID) {
  // PAK files???

  // Try the same search path that we were already searching on.
  if (FS_FindNextFile(pFindData->findHandle, &pFindData->findData)) {
    if (pFoundStoreID) *pFoundStoreID = pFindData->m_CurrentStoreID;

    return true;
  }

  // This happens when we searched a full path
  if (pFindData->currentSearchPathID < 0) return false;

  pFindData->currentSearchPathID++;

  if (pFindData->findHandle != INVALID_HANDLE_VALUE) {
    FS_FindClose(pFindData->findHandle);
  }
  pFindData->findHandle = INVALID_HANDLE_VALUE;

  int c = m_SearchPaths.Count();
  for (; pFindData->currentSearchPathID < c; ++pFindData->currentSearchPathID) {
    CSearchPath *pSearchPath = &m_SearchPaths[pFindData->currentSearchPathID];

    // TODO(d.rattman): Should this work with PAK files?
    if (pSearchPath->GetPackFile()) continue;

    if (FilterByPathID(pSearchPath, pFindData->m_FilterPathID)) continue;

    // already visited this path
    if (pFindData->m_VisitedSearchPaths.MarkVisit(*pSearchPath)) continue;

    char pTmpFileName[MAX_FILEPATH];
    sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(),
              pFindData->wildCardString.Base());
    Q_FixSlashes(pTmpFileName);
    pFindData->findHandle =
        FS_FindFirstFile(pTmpFileName, &pFindData->findData);
    pFindData->m_CurrentStoreID = pSearchPath->m_storeId;
    if (pFindData->findHandle != INVALID_HANDLE_VALUE) {
      if (pFoundStoreID) *pFoundStoreID = pFindData->m_CurrentStoreID;

      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : handle -
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::FindNext(FileFindHandle_t handle) {
  VPROF_BUDGET("CBaseFileSystem::FindNext", VPROF_BUDGETGROUP_OTHER_FILESYSTEM);
  FindData_t *pFindData = &m_FindData[handle];

  while (1) {
    if (FindNextFileHelper(pFindData, nullptr)) {
      if (pFindData->m_VisitedFiles.Find(pFindData->findData.cFileName) == -1) {
        pFindData->m_VisitedFiles.Insert(pFindData->findData.cFileName, 0);
        return pFindData->findData.cFileName;
      }
    } else {
      return nullptr;
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : handle -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FindIsDirectory(FileFindHandle_t handle) {
  FindData_t *pFindData = &m_FindData[handle];
  return !!(pFindData->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : handle -
//-----------------------------------------------------------------------------
void CBaseFileSystem::FindClose(FileFindHandle_t handle) {
  if ((handle < 0) || (!m_FindData.IsInList(handle))) return;

  FindData_t *pFindData = &m_FindData[handle];
  Assert(pFindData);

  if (pFindData->findHandle != INVALID_HANDLE_VALUE) {
    FS_FindClose(pFindData->findHandle);
  }
  pFindData->findHandle = INVALID_HANDLE_VALUE;

  pFindData->wildCardString.Purge();
  m_FindData.Remove(handle);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pFileName -
//-----------------------------------------------------------------------------
void CBaseFileSystem::GetLocalCopy(const char *pFileName) {
  // do nothing. . everything is local.
}

//-----------------------------------------------------------------------------
// Converts a partial path into a full path
// Relative paths that are pack based are returned as an absolute path
// .../zip?.zip/foo A pack absolute path can be sent back in for opening, and
// the file will be properly detected as pack based and mounted inside the pack.
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::RelativePathToFullPath(
    const char *pFileName, const char *pPathID, char *pFullPath,
    int fullPathBufferSize, PathTypeFilter_t pathFilter,
    PathTypeQuery_t *pPathType) {
  CHECK_DOUBLE_SLASHES(pFileName);

  struct _stat buf;

  if (pPathType) {
    *pPathType = PATH_IS_NORMAL;
  }

  // Fill in the default in case it's not found...
  strcpy_s(pFullPath, fullPathBufferSize, pFileName);

  if (pathFilter == FILTER_NONE) {
    // X360TBD: PC legacy behavior never returned pack paths
    // do legacy behavior to ensure naive callers don't break
    pathFilter = FILTER_CULLPACK;
  }

  CSearchPathsIterator iter(this, &pFileName, pPathID, pathFilter);
  for (CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != nullptr;
       pSearchPath = iter.GetNext()) {
    int dummy;
    int64_t dummy64;

    CPackFile *pPack = pSearchPath->GetPackFile();
    if (pPack) {
      if (pPack->FindFile(pFileName, dummy, dummy64, dummy)) {
        if (pPathType) {
          if (pPack->m_bIsMapPath) {
            *pPathType |= PATH_IS_MAPPACKFILE;
          } else {
            *pPathType |= PATH_IS_PACKFILE;
          }
          if (pSearchPath->m_bIsRemotePath) {
            *pPathType |= PATH_IS_REMOTE;
          }
        }

        // form an encoded absolute path that can be decoded by our FS as pak
        // based
        strcpy_s(pFullPath, fullPathBufferSize, pPack->m_ZipName.String());
        V_AppendSlash(pFullPath, fullPathBufferSize - strlen(pFullPath));
        usize len = strlen(pFullPath);
        strcpy_s(pFullPath + len, fullPathBufferSize - len, pFileName);

        return pFullPath;
      }

      continue;
    }

    char pTmpFileName[MAX_FILEPATH];
    sprintf_s(pTmpFileName, "%s%s", pSearchPath->GetPathString(), pFileName);
    Q_FixSlashes(pTmpFileName);
    if (FS_stat(pTmpFileName, &buf) != -1) {
      strcpy_s(pFullPath, fullPathBufferSize, pTmpFileName);
      if (pPathType && pSearchPath->m_bIsRemotePath) {
        *pPathType |= PATH_IS_REMOTE;
      }
      return pFullPath;
    }
  }

  // not found
  return nullptr;
}

const char *CBaseFileSystem::GetLocalPath(const char *pFileName,
                                          char *pLocalPath,
                                          int localPathBufferSize) {
  CHECK_DOUBLE_SLASHES(pFileName);

  return RelativePathToFullPath(pFileName, nullptr, pLocalPath,
                                localPathBufferSize);
}

//-----------------------------------------------------------------------------
// Returns true on success, otherwise false if it can't be resolved
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePathEx(const char *pFullPath,
                                               const char *pPathId,
                                               char *pRelative, int nMaxLen) {
  CHECK_DOUBLE_SLASHES(pFullPath);

  usize nInlen = strlen(pFullPath);
  if (nInlen == 0) {
    pRelative[0] = 0;
    return false;
  }

  strcpy_s(pRelative, nMaxLen, pFullPath);

  char pInPath[MAX_FILEPATH];
  strcpy_s(pInPath, pFullPath);
#ifdef _WIN32
  Q_strlower(pInPath);
#endif
  Q_FixSlashes(pInPath);

  CUtlSymbol lookup;
  if (pPathId) {
    lookup = g_PathIDTable.AddString(pPathId);
  }

  int c = m_SearchPaths.Count();
  for (int i = 0; i < c; i++) {
    // TODO(d.rattman): Should this work with embedded pak files?
    if (m_SearchPaths[i].GetPackFile() &&
        m_SearchPaths[i].GetPackFile()->m_bIsMapPath)
      continue;

    // Skip paths that are not on the specified search path
    if (FilterByPathID(&m_SearchPaths[i], lookup)) continue;

    char pSearchBase[MAX_FILEPATH];
    strcpy_s(pSearchBase, m_SearchPaths[i].GetPathString());
#ifdef _WIN32
    Q_strlower(pSearchBase);
#endif
    Q_FixSlashes(pSearchBase);
    usize nSearchLen = strlen(pSearchBase);
    if (_strnicmp(pSearchBase, pInPath, nSearchLen)) continue;

    strcpy_s(pRelative, nMaxLen, &pInPath[nSearchLen]);
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------
// Obsolete version
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePath(const char *pFullPath,
                                             char *pRelative, int nMaxLen) {
  return FullPathToRelativePathEx(pFullPath, nullptr, pRelative, nMaxLen);
}

//-----------------------------------------------------------------------------
// Deletes a file
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveFile(char const *pRelativePath,
                                 const char *pathID) {
  CHECK_DOUBLE_SLASHES(pRelativePath);

  // Allow for UNC-type syntax to specify the path ID.
  char tem_path_id[SOURCE_MAX_PATH];
  ParsePathID(pRelativePath, pathID, tem_path_id);

  // Opening for write or append uses Write Path
  char file_path[SOURCE_MAX_PATH];
  if (Q_IsAbsolutePath(pRelativePath)) {
    strcpy_s(file_path, pRelativePath);
  } else {
    ComputeFullWritePath(file_path, sizeof(file_path), pRelativePath, pathID);
  }

  const int unlink_code{_unlink(file_path)};
  if (unlink_code != EOK) {
    Warning(FILESYSTEM_WARNING, "Unable to remove file %s: %s.\n", file_path,
            source::posix_errno_info_last_error().description);
  }
}

//-----------------------------------------------------------------------------
// Renames a file
//-----------------------------------------------------------------------------
bool CBaseFileSystem::RenameFile(char const *pOldPath, char const *pNewPath,
                                 const char *pathID) {
  Assert(pOldPath && pNewPath);

  CHECK_DOUBLE_SLASHES(pOldPath);
  CHECK_DOUBLE_SLASHES(pNewPath);

  // Allow for UNC-type syntax to specify the path ID.
  char pPathIdCopy[SOURCE_MAX_PATH];
  const char *pOldPathId = pathID;
  if (pathID) {
    strcpy_s(pPathIdCopy, pathID);
    pOldPathId = pPathIdCopy;
  }

  char tempOldPathID[SOURCE_MAX_PATH];
  ParsePathID(pOldPath, pOldPathId, tempOldPathID);
  Assert(pOldPathId);

  // Allow for UNC-type syntax to specify the path ID.
  char tempNewPathID[SOURCE_MAX_PATH];
  ParsePathID(pNewPath, pathID, tempNewPathID);
  Assert(pathID);

  char pNewFileName[SOURCE_MAX_PATH];
  char szScratchFileName[SOURCE_MAX_PATH];

  // The source file may be in a fallback directory, so just resolve the actual
  // path, don't assume pathid...
  RelativePathToFullPath(pOldPath, pOldPathId, szScratchFileName,
                         sizeof(szScratchFileName));

  // Figure out the dest path
  if (!Q_IsAbsolutePath(pNewPath)) {
    ComputeFullWritePath(pNewFileName, sizeof(pNewFileName), pNewPath, pathID);
  } else {
    strcpy_s(pNewFileName, pNewPath);
  }

  // Make sure the directory exitsts, too
  char pPathOnly[SOURCE_MAX_PATH];
  strcpy_s(pPathOnly, pNewFileName);
  Q_StripFilename(pPathOnly);
  CreateDirHierarchy(pPathOnly, pathID);

  // Now copy the file over
  const int rename_code{rename(szScratchFileName, pNewFileName)};
  if (rename_code != EOK) {
    Warning(FILESYSTEM_WARNING, "Unable to rename file %s to %s: %s.\n",
            szScratchFileName, pNewFileName,
            source::posix_errno_info_last_error().description);
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : **ppdir -
//-----------------------------------------------------------------------------
bool CBaseFileSystem::GetCurrentDirectory(char *pDirectory, int maxlen) {
#if defined(_WIN32) && !defined(_X360)
  if (!::GetCurrentDirectoryA(maxlen, pDirectory))
#elif defined(OS_POSIX) || defined(_X360)
  if (!getcwd(pDirectory, maxlen))
#endif
    return false;

  Q_FixSlashes(pDirectory);

  // Strip the last slash
  int len = strlen(pDirectory);
  if (pDirectory[len - 1] == CORRECT_PATH_SEPARATOR) {
    pDirectory[len - 1] = 0;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pfnWarning - warning function callback
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningFunc(void (*pfnWarning)(const char *fmt, ...)) {
  m_pfnWarning = pfnWarning;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : level -
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningLevel(FileWarningLevel_t level) {
  m_fwLevel = level;
}

const FileSystemStatistics *CBaseFileSystem::GetFilesystemStatistics() {
  return &m_Stats;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : level -
//			*fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseFileSystem::Warning(FileWarningLevel_t level, const char *fmt, ...) {
  if (level > m_fwLevel) return;

  if ((fs_warning_mode.GetInt() == 1 && !ThreadInMainThread()) ||
      (fs_warning_mode.GetInt() == 2 && ThreadInMainThread()))
    return;

  va_list argptr;
  char warningtext[4096];

  va_start(argptr, fmt);
  vsprintf_s(warningtext, fmt, argptr);
  va_end(argptr);

  // Dump to stdio
  fprintf(stderr, "%s", warningtext);
  if (m_pfnWarning) {
    (*m_pfnWarning)(warningtext);
  } else {
#ifdef _WIN32
    Plat_DebugString(warningtext);
#endif
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile() {
  m_pFile = nullptr;
  m_pName = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::~COpenedFile() { delete[] m_pName; }

//-----------------------------------------------------------------------------
// Purpose:
// Input  : src -
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile(const COpenedFile &src) {
  m_pFile = src.m_pFile;
  if (src.m_pName) {
    usize len = strlen(src.m_pName) + 1;
    m_pName = new char[len];
    strcpy_s(m_pName, len, src.m_pName);
  } else {
    m_pName = nullptr;
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : src -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::COpenedFile::operator==(
    const CBaseFileSystem::COpenedFile &src) const {
  return src.m_pFile == m_pFile;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *name -
//-----------------------------------------------------------------------------
void CBaseFileSystem::COpenedFile::SetName(char const *name) {
  delete[] m_pName;
  usize len = strlen(name) + 1;
  m_pName = new char[len];
  strcpy_s(m_pName, len, name);
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : char
//-----------------------------------------------------------------------------
char const *CBaseFileSystem::COpenedFile::GetName() {
  return m_pName ? m_pName : "???";
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::CSearchPath() {
  m_Path = g_PathIDTable.AddString("");
  m_pDebugPath = "";

  m_storeId = 0;
  m_pPackFile = nullptr;
  m_pPathIDInfo = nullptr;
  m_bIsRemotePath = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::~CSearchPath() {
  if (m_pPackFile) {
    m_pPackFile->Release();
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *
CBaseFileSystem::CSearchPathsIterator::GetFirst() {
  if (m_SearchPaths.Count()) {
    m_visits.Reset();
    m_iCurrent = -1;
    return GetNext();
  }
  return &m_EmptySearchPath;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::CSearchPathsIterator::GetNext() {
  CSearchPath *pSearchPath = nullptr;

  for (m_iCurrent++; m_iCurrent < m_SearchPaths.Count(); m_iCurrent++) {
    pSearchPath = &m_SearchPaths[m_iCurrent];

    if (m_PathTypeFilter == FILTER_CULLPACK && pSearchPath->GetPackFile())
      continue;

    if (m_PathTypeFilter == FILTER_CULLNONPACK && !pSearchPath->GetPackFile())
      continue;

    if (CBaseFileSystem::FilterByPathID(pSearchPath, m_pathID)) continue;

    if (!m_visits.MarkVisit(*pSearchPath)) break;
  }

  if (m_iCurrent < m_SearchPaths.Count()) {
    return pSearchPath;
  }

  return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Load/unload a DLL
//-----------------------------------------------------------------------------
CSysModule *CBaseFileSystem::LoadModule(const char *file_name,
                                        const char *pPathID,
                                        bool bValidatedDllOnly) {
  CHECK_DOUBLE_SLASHES(file_name);

  LogFileAccess(file_name);

  // default to the bin dir
  if (!pPathID) pPathID = "EXECUTABLE_PATH";

  char tempPathID[SOURCE_MAX_PATH];
  ParsePathID(file_name, pPathID, tempPathID);

  CUtlSymbol lookup = g_PathIDTable.AddString(pPathID);

  // a pathID has been specified, find the first match in the path list
  int c = m_SearchPaths.Count();
  for (int i = 0; i < c; i++) {
    // pak files don't have modules
    if (m_SearchPaths[i].GetPackFile()) continue;
    if (FilterByPathID(&m_SearchPaths[i], lookup)) continue;

    // append the path to this dir.
    sprintf_s(tempPathID, "%s%s", m_SearchPaths[i].GetPathString(), file_name);

    CSysModule *pModule = Sys_LoadModule(tempPathID);
    // we found the binary in one of our search paths
    if (pModule) return pModule;
  }

  // Do not try to load by name to prevent dll hijacking, just fail.
  return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseFileSystem::UnloadModule(CSysModule *pModule) {
  Sys_UnloadModule(pModule);
}

//-----------------------------------------------------------------------------
// Purpose: Adds a filesystem logging function
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddLoggingFunc(FileSystemLoggingFunc_t logFunc) {
  Assert(!m_LogFuncs.IsValidIndex(m_LogFuncs.Find(logFunc)));
  m_LogFuncs.AddToTail(logFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Removes a filesystem logging function
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveLoggingFunc(FileSystemLoggingFunc_t logFunc) {
  m_LogFuncs.FindAndRemove(logFunc);
}

//-----------------------------------------------------------------------------
// Make sure that slashes are of the right kind and that there is a slash at the
// end of the filename.
// WARNING!!: assumes that you have an extra byte allocated in the case that you
// need a slash at the end.
//-----------------------------------------------------------------------------
static void AddSeperatorAndFixPath(char *str) {
  char *lastChar = &str[strlen(str) - 1];
  if (*lastChar != CORRECT_PATH_SEPARATOR &&
      *lastChar != INCORRECT_PATH_SEPARATOR) {
    lastChar[1] = CORRECT_PATH_SEPARATOR;
    lastChar[2] = '\0';
  }
  Q_FixSlashes(str);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pFileName -
// Output : FileNameHandle_t
//-----------------------------------------------------------------------------
FileNameHandle_t CBaseFileSystem::FindOrAddFileName(char const *pFileName) {
  return m_FileNames.FindOrAddFileName(pFileName);
}

FileNameHandle_t CBaseFileSystem::FindFileName(char const *pFileName) {
  return m_FileNames.FindFileName(pFileName);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : handle -
// Output : char const
//-----------------------------------------------------------------------------
bool CBaseFileSystem::String(const FileNameHandle_t &handle, char *buf,
                             int buflen) {
  return m_FileNames.String(handle, buf, buflen);
}

int CBaseFileSystem::GetPathIndex(const FileNameHandle_t &handle) {
  return m_FileNames.PathIndex(handle);
}

CBaseFileSystem::CPathIDInfo *CBaseFileSystem::FindOrAddPathIDInfo(
    const CUtlSymbol &id, int bByRequestOnly) {
  for (int i = 0; i < m_PathIDInfos.Count(); i++) {
    CBaseFileSystem::CPathIDInfo *pInfo = m_PathIDInfos[i];
    if (pInfo->GetPathID() == id) {
      if (bByRequestOnly != -1) {
        pInfo->m_bByRequestOnly = (bByRequestOnly != 0);
      }
      return pInfo;
    }
  }

  // Add a new one.
  CBaseFileSystem::CPathIDInfo *pInfo = new CBaseFileSystem::CPathIDInfo;
  m_PathIDInfos.AddToTail(pInfo);
  pInfo->SetPathID(id);
  pInfo->m_bByRequestOnly = (bByRequestOnly == 1);
  return pInfo;
}

void CBaseFileSystem::MarkPathIDByRequestOnly(const char *pPathID,
                                              bool bRequestOnly) {
  FindOrAddPathIDInfo(g_PathIDTable.AddString(pPathID), bRequestOnly);
}

#if defined(TRACK_BLOCKING_IO)

void CBaseFileSystem::EnableBlockingFileAccessTracking(bool state) {
  m_bBlockingFileAccessReportingEnabled = state;
}

bool CBaseFileSystem::IsBlockingFileAccessEnabled() const {
  return m_bBlockingFileAccessReportingEnabled;
}

IBlockingFileItemList *CBaseFileSystem::RetrieveBlockingFileAccessInfo() {
  Assert(m_pBlockingItems);
  return m_pBlockingItems;
}

void CBaseFileSystem::RecordBlockingFileAccess(bool synchronous,
                                               const FileBlockingItem &item) {
  AUTO_LOCK(m_BlockingFileMutex);

  // Not tracking anything
  if (!m_bBlockingFileAccessReportingEnabled) return;

  if (synchronous && !m_bAllowSynchronousLogging &&
      (item.m_ItemType == FILESYSTEM_BLOCKING_SYNCHRONOUS))
    return;

  // Track it
  m_pBlockingItems->Add(item);
}

bool CBaseFileSystem::SetAllowSynchronousLogging(bool state) {
  bool oldState = m_bAllowSynchronousLogging;
  m_bAllowSynchronousLogging = state;
  return oldState;
}

void CBaseFileSystem::BlockingFileAccess_EnterCriticalSection() {
  m_BlockingFileMutex.Lock();
}

void CBaseFileSystem::BlockingFileAccess_LeaveCriticalSection() {
  m_BlockingFileMutex.Unlock();
}

#endif  // TRACK_BLOCKING_IO

bool CBaseFileSystem::GetFileTypeForFullPath(char const *pFullPath,
                                             wchar_t *buf,
                                             size_t bufSizeInBytes) {
  const size_t buffer_size_in_words{bufSizeInBytes / sizeof(wchar_t)};

#ifndef OS_POSIX
  wchar_t unc_path[512];
  ::MultiByteToWideChar(CP_UTF8, 0, pFullPath, -1, unc_path,
                        SOURCE_ARRAYSIZE(unc_path));
  unc_path[SOURCE_ARRAYSIZE(unc_path)] = L'\0';

  SHFILEINFOW info{0};
  DWORD_PTR return_code =
      SHGetFileInfoW(unc_path, 0, &info, sizeof(info), SHGFI_TYPENAME);
  if (return_code) {
    wcsncpy_s(buf, buffer_size_in_words, info.szTypeName, buffer_size_in_words);
    return true;
  }
#endif

  char ext[32];
  Q_ExtractFileExtension(pFullPath, ext, SOURCE_ARRAYSIZE(ext));
  _snwprintf_s(buf, buffer_size_in_words, buffer_size_in_words - 1, L".%S",
               ext);
  return false;
}

bool CBaseFileSystem::GetOptimalIOConstraints(FileHandle_t hFile,
                                              unsigned *pOffsetAlign,
                                              unsigned *pSizeAlign,
                                              unsigned *pBufferAlign) {
  if (pOffsetAlign) *pOffsetAlign = 1;
  if (pSizeAlign) *pSizeAlign = 1;
  if (pBufferAlign) *pBufferAlign = 1;
  return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a file handle
// Input  : base file system handle
// Output :
//-----------------------------------------------------------------------------
CFileHandle::CFileHandle(CBaseFileSystem *fs) { Init(fs); }

CFileHandle::~CFileHandle() {
  Assert(IsValid());
#if !defined(_RETAIL)
  delete[] m_pszTrueFileName;
#endif

  if (m_pPackFileHandle) {
    delete m_pPackFileHandle;
    m_pPackFileHandle = nullptr;
  }

  if (m_pFile) {
    m_fs->Trace_FClose(m_pFile);
    m_pFile = nullptr;
  }

  m_nMagic = FREE_MAGIC;
}

void CFileHandle::Init(CBaseFileSystem *fs) {
  m_nMagic = MAGIC;
  m_pFile = nullptr;
  m_nLength = 0;
  m_type = FT_NORMAL;
  m_pPackFileHandle = nullptr;

  m_fs = fs;

#if !defined(_RETAIL)
  m_pszTrueFileName = 0;
#endif
}

bool CFileHandle::IsValid() { return (m_nMagic == MAGIC); }

int CFileHandle::GetSectorSize() {
  Assert(IsValid());

  if (m_pFile) {
    return m_fs->FS_GetSectorSize(m_pFile);
  }

  return m_pPackFileHandle ? m_pPackFileHandle->GetSectorSize() : -1;
}

bool CFileHandle::IsOK() {
  if (m_pFile) {
    return (IsValid() && m_fs->FS_ferror(m_pFile) == 0);
  }

  if (m_pPackFileHandle) {
    return IsValid();
  }

  m_fs->Warning(
      FILESYSTEM_WARNING,
      "FS:  Tried to IsOk nullptr file pointer inside valid file handle!\n");
  return false;
}

void CFileHandle::Flush() {
  Assert(IsValid());

  if (m_pFile) {
    m_fs->FS_fflush(m_pFile);
  }
}

void CFileHandle::SetBufferSize(unsigned int nBytes) {
  Assert(IsValid());

  if (m_pFile) {
    m_fs->FS_setbufsize(m_pFile, nBytes);
  } else if (m_pPackFileHandle) {
    m_pPackFileHandle->SetBufferSize(nBytes);
  }
}

int CFileHandle::Read(void *pBuffer, int nLength) {
  Assert(IsValid());
  return Read(pBuffer, -1, nLength);
}

int CFileHandle::Read(void *pBuffer, int nDestSize, int nLength) {
  Assert(IsValid());

  // Is this a regular file or a pack file?
  if (m_pFile) {
    return m_fs->FS_fread(pBuffer, nDestSize, nLength, m_pFile);
  } else if (m_pPackFileHandle) {
    // Pack file handle handles clamping all the reads:
    return m_pPackFileHandle->Read(pBuffer, nDestSize, nLength);
  }

  return 0;
}

int CFileHandle::Write(const void *pBuffer, int nLength) {
  Assert(IsValid());

  if (!m_pFile) {
    m_fs->Warning(
        FILESYSTEM_WARNING,
        "FS:  Tried to Write nullptr file pointer inside valid file handle!\n");
    return 0;
  }

  size_t nBytesWritten = m_fs->FS_fwrite((void *)pBuffer, nLength, m_pFile);

  m_fs->Trace_FWrite(nBytesWritten, m_pFile);

  return nBytesWritten;
}

int CFileHandle::Seek(int64_t nOffset, int nWhence) {
  Assert(IsValid());

  if (m_pFile) {
    m_fs->FS_fseek(m_pFile, nOffset, nWhence);
    // TODO - FS_fseek should return the resultant offset
    return 0;
  } else if (m_pPackFileHandle) {
    return m_pPackFileHandle->Seek(nOffset, nWhence);
  }

  return -1;
}

int CFileHandle::Tell() {
  Assert(IsValid());

  if (m_pFile) {
    return m_fs->FS_ftell(m_pFile);
  } else if (m_pPackFileHandle) {
    return m_pPackFileHandle->Tell();
  }

  return -1;
}

int CFileHandle::Size() {
  Assert(IsValid());

  int nReturnedSize = -1;

  if (m_pFile) {
    nReturnedSize = m_nLength;
  } else if (m_pPackFileHandle) {
    nReturnedSize = m_pPackFileHandle->Size();
  }

  return nReturnedSize;
}

int64_t CFileHandle::AbsoluteBaseOffset() {
  Assert(IsValid());

  if (m_pFile) {
    return 0;
  } else {
    return m_pPackFileHandle->AbsoluteBaseOffset();
  }
}

bool CFileHandle::EndOfFile() {
  Assert(IsValid());

  return (Tell() >= Size());
}
