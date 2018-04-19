// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include "zip_utils.h"

#include "base/include/base_types.h"
#include "tier0/include/platform.h"
#include "tier1/byteswap.h"
#include "tier1/checksum_crc.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"
#include "zip_uncompressed.h"

// Data descriptions for uint8_t swapping - only needed
// for structures that are written to file for use by the game.
BEGIN_BYTESWAP_DATADESC(ZIP_EndOfCentralDirRecord)
  DEFINE_FIELD(signature, FIELD_INTEGER),
      DEFINE_FIELD(numberOfThisDisk, FIELD_SHORT),
      DEFINE_FIELD(numberOfTheDiskWithStartOfCentralDirectory, FIELD_SHORT),
      DEFINE_FIELD(nCentralDirectoryEntries_ThisDisk, FIELD_SHORT),
      DEFINE_FIELD(nCentralDirectoryEntries_Total, FIELD_SHORT),
      DEFINE_FIELD(centralDirectorySize, FIELD_INTEGER),
      DEFINE_FIELD(startOfCentralDirOffset, FIELD_INTEGER),
      DEFINE_FIELD(commentLength, FIELD_SHORT),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_FileHeader)
  DEFINE_FIELD(signature, FIELD_INTEGER),
      DEFINE_FIELD(versionMadeBy, FIELD_SHORT),
      DEFINE_FIELD(versionNeededToExtract, FIELD_SHORT),
      DEFINE_FIELD(flags, FIELD_SHORT),
      DEFINE_FIELD(compressionMethod, FIELD_SHORT),
      DEFINE_FIELD(lastModifiedTime, FIELD_SHORT),
      DEFINE_FIELD(lastModifiedDate, FIELD_SHORT),
      DEFINE_FIELD(crc32, FIELD_INTEGER),
      DEFINE_FIELD(compressedSize, FIELD_INTEGER),
      DEFINE_FIELD(uncompressedSize, FIELD_INTEGER),
      DEFINE_FIELD(fileNameLength, FIELD_SHORT),
      DEFINE_FIELD(extraFieldLength, FIELD_SHORT),
      DEFINE_FIELD(fileCommentLength, FIELD_SHORT),
      DEFINE_FIELD(diskNumberStart, FIELD_SHORT),
      DEFINE_FIELD(internalFileAttribs, FIELD_SHORT),
      DEFINE_FIELD(externalFileAttribs, FIELD_INTEGER),
      DEFINE_FIELD(relativeOffsetOfLocalHeader, FIELD_INTEGER),
END_BYTESWAP_DATADESC()
#if !defined(SWDS)

BEGIN_BYTESWAP_DATADESC(ZIP_LocalFileHeader)
  DEFINE_FIELD(signature, FIELD_INTEGER),
      DEFINE_FIELD(versionNeededToExtract, FIELD_SHORT),
      DEFINE_FIELD(flags, FIELD_SHORT),
      DEFINE_FIELD(compressionMethod, FIELD_SHORT),
      DEFINE_FIELD(lastModifiedTime, FIELD_SHORT),
      DEFINE_FIELD(lastModifiedDate, FIELD_SHORT),
      DEFINE_FIELD(crc32, FIELD_INTEGER),
      DEFINE_FIELD(compressedSize, FIELD_INTEGER),
      DEFINE_FIELD(uncompressedSize, FIELD_INTEGER),
      DEFINE_FIELD(fileNameLength, FIELD_SHORT),
      DEFINE_FIELD(extraFieldLength, FIELD_SHORT),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_PreloadHeader)
  DEFINE_FIELD(Version, FIELD_INTEGER),
      DEFINE_FIELD(DirectoryEntries, FIELD_INTEGER),
      DEFINE_FIELD(PreloadDirectoryEntries, FIELD_INTEGER),
      DEFINE_FIELD(Alignment, FIELD_INTEGER),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC(ZIP_PreloadDirectoryEntry)
  DEFINE_FIELD(Length, FIELD_INTEGER), DEFINE_FIELD(DataOffset, FIELD_INTEGER),
END_BYTESWAP_DATADESC()

// For >2 GB File Support
class Win32File {
 public:
  static HANDLE CreateTempFile(CUtlString &WritePath, CUtlString &FileName) {
    char tmp_file_path[SOURCE_MAX_PATH];
    if (WritePath.IsEmpty()) {
      // use a safe name in the cwd
      char tmp_name_with_back_slash[L_tmpnam_s];
      errno_t err = tmpnam_s(tmp_name_with_back_slash);
      if (err != 0 || !tmp_name_with_back_slash[0]) return INVALID_HANDLE_VALUE;

      char *tmp_name_only = tmp_name_with_back_slash[0] == '\\'
                                ? tmp_name_with_back_slash + 1
                                : tmp_name_with_back_slash;

      if (tmp_name_only[strlen(tmp_name_only) - 1] == '.') {
        tmp_name_only[strlen(tmp_name_only) - 1] = '\0';
      }

      sprintf_s(tmp_file_path, "_%s.tmp", tmp_name_only);
    } else {
      // generate safe name at the desired prefix
      char unique_file_path[SOURCE_MAX_PATH];
      SYSTEMTIME sysTime;
      GetLocalTime(&sysTime);

      sprintf_s(unique_file_path, "%hu_%hu_%hu_%hu_%hu.tmp", sysTime.wDay,
                sysTime.wHour, sysTime.wMinute, sysTime.wSecond,
                sysTime.wMilliseconds);
      V_ComposeFileName(WritePath.String(), unique_file_path, tmp_file_path,
                        SOURCE_ARRAYSIZE(tmp_file_path));
    }

    FileName = tmp_file_path;
    HANDLE hFile =
        CreateFile(tmp_file_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    return hFile;
  }

  static long long FileSeek(HANDLE hFile, long long distance,
                            DWORD move_method) {
    LARGE_INTEGER li;

    li.QuadPart = distance;
    li.LowPart = SetFilePointer(hFile, li.LowPart, &li.HighPart, move_method);
    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
      li.QuadPart = -1;
    }

    return li.QuadPart;
  }

  static long long FileTell(HANDLE hFile) {
    return FileSeek(hFile, 0, FILE_CURRENT);
  }

  static bool FileRead(HANDLE hFile, void *pBuffer, DWORD size) {
    DWORD bytes_read;
    return ReadFile(hFile, pBuffer, size, &bytes_read, nullptr) &&
           bytes_read == size;
  }

  static bool FileWrite(HANDLE hFile, void *pBuffer, DWORD size) {
    DWORD bytes_written;
    return WriteFile(hFile, pBuffer, size, &bytes_written, nullptr) &&
           bytes_written == size;
  }
};

// Purpose: Interface to allow abstraction of zip file output methods, and
// avoid duplication of code. Files may be written to a CUtlBuffer or a
// file stream.
the_interface IWriteStream {
 public:
  virtual void Put(const void *memory, usize size) = 0;
  virtual usize Tell() = 0;
};

// Purpose: Wrapper for CUtlBuffer methods.
class BufferWriteStream : public IWriteStream {
 public:
  BufferWriteStream(CUtlBuffer &buffer) : buffer_{&buffer} {}

  void Put(const void *memory, usize size) override {
    buffer_->Put(memory, size);
  }
  usize Tell() override { return buffer_->TellPut(); }

 private:
  CUtlBuffer *const buffer_;
};

// Purpose: Wrapper for file I/O methods.
class FileWriteStream : public IWriteStream {
 public:
  FileWriteStream(FILE *file_desc)
      : file_desc_{file_desc}, file_handle_{INVALID_HANDLE_VALUE} {}
  FileWriteStream(HANDLE file_handle)
      : file_desc_{nullptr}, file_handle_{file_handle} {}

  void Put(const void *memory, usize size) override {
    if (file_desc_) {
      fwrite(memory, size, 1, file_desc_);
    } else {
      DWORD numBytesWritten;
      WriteFile(file_handle_, memory, size, &numBytesWritten, nullptr);
    }
  }

  usize Tell() override {
    if (file_desc_) {
      return ftell(file_desc_);
    }

    return Win32File::FileTell(file_handle_);
  }

 private:
  FILE *const file_desc_;
  HANDLE const file_handle_;
};

// Purpose: Container for modifiable pak file which is embedded inside the .bsp
// file itself. It's used to allow one-off files to be stored local to the map
// and it is hooked into the file system as an override for searching for named
// files.
class ZipFile {
 public:
  // Construction
  ZipFile(const char *pDiskCacheWritePath, bool bSortByName);
  ~ZipFile() {
    m_bUseDiskCacheForWrites = false;
    Reset();
  }

  // Public API
  // Clear all existing data
  void Reset() {
    m_Files.RemoveAll();

    if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
      CloseHandle(m_hDiskCacheWriteFile);
      DeleteFile(m_DiskCacheName.String());
      m_hDiskCacheWriteFile = INVALID_HANDLE_VALUE;
    }

    if (m_bUseDiskCacheForWrites) {
      m_hDiskCacheWriteFile =
          Win32File::CreateTempFile(m_DiskCacheWritePath, m_DiskCacheName);
    }
  }

  // Add file to zip under relative name
  void AddFileToZip(const char *relativename, const char *fullpath);

  // Delete file from zip
  void RemoveFileFromZip(const char *relativename);

  // Add buffer to zip as a file with given name
  void AddBufferToZip(const char *relativename, void *data, int length,
                      bool bTextMode);

  // Check if a file already exists in the zip.
  bool FileExistsInZip(const char *relativename);

  // Reads a file from a zip file
  bool ReadFileFromZip(const char *relativename, bool bTextMode,
                       CUtlBuffer &buf);
  bool ReadFileFromZip(HANDLE hZipFile, const char *relativename,
                       bool bTextMode, CUtlBuffer &buf);

  // Initialize the zip file from a buffer
  void ParseFromBuffer(void *buffer, int bufferlength) {
    // Throw away old data
    Reset();

    // Initialize a buffer
    CUtlBuffer buf(0, bufferlength + 1);  // +1 for 0 termination

    // need to swap bytes, so set the buffer opposite the machine's endian
    buf.ActivateByteSwapping(m_Swap.IsSwappingBytes());

    buf.Put(buffer, bufferlength);

    buf.SeekGet(CUtlBuffer::SEEK_TAIL, 0);
    unsigned int fileLen = buf.TellGet();

    // Start from beginning
    buf.SeekGet(CUtlBuffer::SEEK_HEAD, 0);

    unsigned int offset;
    ZIP_EndOfCentralDirRecord rec = {0};

    bool bFoundEndOfCentralDirRecord = false;
    for (offset = fileLen - sizeof(ZIP_EndOfCentralDirRecord) + 1; offset >= 1;
         offset--) {
      buf.SeekGet(CUtlBuffer::SEEK_HEAD, offset - 1);
      buf.GetObjects(&rec);
      if (rec.signature == PKID(5, 6)) {
        bFoundEndOfCentralDirRecord = true;

        // Set any xzip configuration
        if (rec.commentLength) {
          char commentString[128];
          int commentLength =
              std::min((usize)rec.commentLength, sizeof(commentString));
          buf.Get(commentString, commentLength);
          commentString[commentLength] = '\0';
          ParseXZipCommentString(commentString);
        }
        break;
      } else {
        // wrong record
        rec.nCentralDirectoryEntries_Total = 0;
      }
    }
    Assert(bFoundEndOfCentralDirRecord);

    // Make sure there are some files to parse
    int numzipfiles = rec.nCentralDirectoryEntries_Total;
    if (numzipfiles <= 0) {
      // No files
      return;
    }

    buf.SeekGet(CUtlBuffer::SEEK_HEAD, rec.startOfCentralDirOffset);

    // Allocate space for directory
    TmpFileInfo_t *newfiles = new TmpFileInfo_t[numzipfiles];
    Assert(newfiles);

    // build directory
    int i;
    for (i = 0; i < rec.nCentralDirectoryEntries_Total; i++) {
      ZIP_FileHeader zipFileHeader;
      buf.GetObjects(&zipFileHeader);
      Assert(zipFileHeader.signature == PKID(1, 2));
      Assert(zipFileHeader.compressionMethod == 0);

      char tmpString[1024];
      buf.Get(tmpString, zipFileHeader.fileNameLength);
      tmpString[zipFileHeader.fileNameLength] = '\0';
      Q_strlower(tmpString);

      // can determine actual filepos, assuming a well formed zip
      newfiles[i].m_Name = tmpString;
      newfiles[i].filelen = zipFileHeader.compressedSize;
      newfiles[i].filepos = zipFileHeader.relativeOffsetOfLocalHeader +
                            sizeof(ZIP_LocalFileHeader) +
                            zipFileHeader.fileNameLength +
                            zipFileHeader.extraFieldLength;

      int nextOffset;
      if (m_bCompatibleFormat) {
        nextOffset =
            zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
      } else {
        nextOffset = 0;
      }
      buf.SeekGet(CUtlBuffer::SEEK_CURRENT, nextOffset);
    }

    // Insert current data into rb tree
    for (i = 0; i < numzipfiles; i++) {
      CZipEntry e;
      e.m_Name = newfiles[i].m_Name;
      e.m_Length = newfiles[i].filelen;

      // Make sure length is reasonable
      if (e.m_Length > 0) {
        e.m_pData = malloc(e.m_Length);

        // Copy in data
        buf.SeekGet(CUtlBuffer::SEEK_HEAD, newfiles[i].filepos);
        buf.Get(e.m_pData, e.m_Length);
      } else {
        e.m_pData = nullptr;
      }

      // Add to tree
      m_Files.Insert(e);
    }

    // Through away directory
    delete[] newfiles;
  }
  HANDLE ParseFromDisk(const char *pFilename) {
    HANDLE hFile =
        CreateFile(pFilename, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile) {
      // not found
      return nullptr;
    }

    unsigned int fileLen = Win32File::FileSeek(hFile, 0, FILE_END);
    Win32File::FileSeek(hFile, 0, FILE_BEGIN);
    if (fileLen < sizeof(ZIP_EndOfCentralDirRecord)) {
      // bad format
      CloseHandle(hFile);
      return nullptr;
    }

    // need to get the central dir
    unsigned int offset;
    ZIP_EndOfCentralDirRecord rec = {0};
    for (offset = fileLen - sizeof(ZIP_EndOfCentralDirRecord) + 1; offset >= 1;
         offset--) {
      Win32File::FileSeek(hFile, offset - 1, FILE_BEGIN);

      Win32File::FileRead(hFile, &rec, sizeof(rec));
      m_Swap.SwapFieldsToTargetEndian(&rec);

      if (rec.signature == PKID(5, 6)) {
        // Set any xzip configuration
        if (rec.commentLength) {
          char commentString[128];
          int commentLength =
              std::min((usize)rec.commentLength, sizeof(commentString));
          Win32File::FileRead(hFile, commentString, commentLength);
          commentString[commentLength] = '\0';
          ParseXZipCommentString(commentString);
        }
        break;
      } else {
        // wrong record
        rec.nCentralDirectoryEntries_Total = 0;
      }
    }

    // Make sure there are some files to parse
    int numZipFiles = rec.nCentralDirectoryEntries_Total;
    if (numZipFiles <= 0) {
      // No files
      CloseHandle(hFile);
      return nullptr;
    }

    Win32File::FileSeek(hFile, rec.startOfCentralDirOffset, FILE_BEGIN);

    // read entire central dir into memory
    CUtlBuffer zipDirBuff(0, rec.centralDirectorySize, 0);
    zipDirBuff.ActivateByteSwapping(m_Swap.IsSwappingBytes());
    Win32File::FileRead(hFile, zipDirBuff.Base(), rec.centralDirectorySize);
    zipDirBuff.SeekPut(CUtlBuffer::SEEK_HEAD, rec.centralDirectorySize);

    // build directory
    for (int i = 0; i < numZipFiles; i++) {
      ZIP_FileHeader zipFileHeader;
      zipDirBuff.GetObjects(&zipFileHeader);

      if (zipFileHeader.signature != PKID(1, 2) ||
          zipFileHeader.compressionMethod != 0) {
        // bad contents
        CloseHandle(hFile);
        return nullptr;
      }

      char fileName[1024];
      zipDirBuff.Get(fileName, zipFileHeader.fileNameLength);
      fileName[zipFileHeader.fileNameLength] = '\0';
      Q_strlower(fileName);

      // can determine actual filepos, assuming a well formed zip
      CZipEntry e;
      e.m_Name = fileName;
      e.m_Length = zipFileHeader.compressedSize;
      e.m_SourceDiskOffset = zipFileHeader.relativeOffsetOfLocalHeader +
                             sizeof(ZIP_LocalFileHeader) +
                             zipFileHeader.fileNameLength +
                             zipFileHeader.extraFieldLength;
      // Add to tree
      m_Files.Insert(e);

      int nextOffset;
      if (m_bCompatibleFormat) {
        nextOffset =
            zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength;
      } else {
        nextOffset = 0;
      }

      zipDirBuff.SeekGet(CUtlBuffer::SEEK_CURRENT, nextOffset);
    }

    return hFile;
  }

  // Estimate the size of the zip file (including header, padding, etc.)
  unsigned int EstimateSize();

  // Print out a directory of files in the zip.
  void PrintDirectory();

  // Use to iterate directory, pass 0 for first element
  // returns nonzero element id with filled buffer, or -1 at list conclusion
  int GetNextFilename(int id, char *pBuffer, int bufferSize, int &fileSize);

  // Write the zip to a buffer
  void SaveToBuffer(CUtlBuffer &buffer);
  // Write the zip to a filestream
  void SaveToDisk(FILE *fout);
  void SaveToDisk(HANDLE hOutFile);

  unsigned int CalculateSize();

  void ForceAlignment(bool aligned, bool bCompatibleFormat,
                      unsigned int alignmentSize) {
    m_bForceAlignment = aligned;
    m_AlignmentSize = alignmentSize;
    m_bCompatibleFormat = bCompatibleFormat;

    if (!aligned) {
      m_AlignmentSize = 0;
    } else if (!IsPowerOfTwo(m_AlignmentSize)) {
      m_AlignmentSize = 0;
    }
  }

  unsigned int GetAlignment() {
    if (!m_bForceAlignment || !m_AlignmentSize) {
      return 0;
    }

    return m_AlignmentSize;
  }

  void SetBigEndian(bool bigEndian) { m_Swap.SetTargetBigEndian(bigEndian); }
  void ActivateByteSwapping(bool bActivate) {
    m_Swap.ActivateByteSwapping(bActivate);
  }

 private:
  enum {
    MAX_FILES_IN_ZIP = 32768,
  };

  typedef struct {
    CUtlSymbol m_Name;
    unsigned int filepos;
    int filelen;
  } TmpFileInfo_t;

  CByteswap m_Swap;
  unsigned int m_AlignmentSize;
  bool m_bForceAlignment;
  bool m_bCompatibleFormat;

  unsigned short CalculatePadding(unsigned int filenameLen, unsigned int pos);
  void SaveDirectory(IWriteStream &stream);
  u16 MakeXZipCommentString(char *pComment, size_t max_length);
  void ParseXZipCommentString(const char *pComment);

  // Internal entry for faster searching, etc.
  class CZipEntry {
   public:
    CZipEntry() {
      m_Name = "";
      m_Length = 0;
      m_pData = nullptr;
      m_ZipOffset = 0;
      m_ZipCRC = 0;
      m_DiskCacheOffset = 0;
      m_SourceDiskOffset = 0;
    }
    ~CZipEntry() { free(m_pData); }

    CZipEntry(const CZipEntry &src) {
      m_Name = src.m_Name;
      m_Length = src.m_Length;

      if (src.m_Length > 0 && src.m_pData) {
        m_pData = malloc(src.m_Length);
        memcpy(m_pData, src.m_pData, src.m_Length);
      } else {
        m_pData = nullptr;
      }

      m_ZipOffset = src.m_ZipOffset;
      m_ZipCRC = src.m_ZipCRC;
      m_DiskCacheOffset = src.m_DiskCacheOffset;
      m_SourceDiskOffset = src.m_SourceDiskOffset;
    }

    // RB tree compare function
    static bool ZipFileLessFunc(CZipEntry const &src1, CZipEntry const &src2) {
      return (src1.m_Name < src2.m_Name);
    }
    static bool ZipFileLessFunc_CaselessSort(CZipEntry const &src1,
                                             CZipEntry const &src2) {
      return _stricmp(src1.m_Name.String(), src2.m_Name.String()) < 0;
    }

    // Name of entry
    CUtlSymbol m_Name;

    // Lenth of data element
    int m_Length;
    // Raw data, could be 0 and data may be in disk write cache
    void *m_pData;

    // Offset in Zip ( set and valid during final write )
    unsigned int m_ZipOffset;
    // CRC of blob ( set and valid during final write )
    CRC32_t m_ZipCRC;

    // Location of data in disk cache
    unsigned int m_DiskCacheOffset;
    unsigned int m_SourceDiskOffset;
  };

  // For fast name lookup and sorting
  CUtlRBTree<CZipEntry, int> m_Files;

  // Used to buffer zip data, instead of ram
  bool m_bUseDiskCacheForWrites;
  HANDLE m_hDiskCacheWriteFile;
  CUtlString m_DiskCacheName;
  CUtlString m_DiskCacheWritePath;
};

ZipFile::ZipFile(const char *pDiskCacheWritePath, bool bSortByName)
    : m_Files{0, 32}, m_DiskCacheWritePath{pDiskCacheWritePath} {
  m_AlignmentSize = 0;
  m_bForceAlignment = false;
  m_bCompatibleFormat = true;

  m_bUseDiskCacheForWrites = (pDiskCacheWritePath != nullptr);
  m_hDiskCacheWriteFile = INVALID_HANDLE_VALUE;

  if (bSortByName) {
    m_Files.SetLessFunc(CZipEntry::ZipFileLessFunc_CaselessSort);
  } else {
    m_Files.SetLessFunc(CZipEntry::ZipFileLessFunc);
  }
}

static int GetLengthOfBinStringAsText(const char *pSrc, int srcSize) {
  const char *pSrcScan = pSrc;
  const char *pSrcEnd = pSrc + srcSize;
  int numChars = 0;
  for (; pSrcScan < pSrcEnd; pSrcScan++) {
    if (*pSrcScan == '\n') {
      numChars += 2;
    } else {
      numChars++;
    }
  }
  return numChars;
}

// Copies text data from a form appropriate for disk to a normal string
static void ReadTextData(const char *pSrc, int nSrcSize, CUtlBuffer &buf) {
  buf.EnsureCapacity(nSrcSize + 1);
  const char *pSrcEnd = pSrc + nSrcSize;
  for (const char *pSrcScan = pSrc; pSrcScan < pSrcEnd; ++pSrcScan) {
    if (*pSrcScan == '\r') {
      if (pSrcScan[1] == '\n') {
        buf.PutChar('\n');
        ++pSrcScan;
        continue;
      }
    }

    buf.PutChar(*pSrcScan);
  }

  // nullptr terminate
  buf.PutChar('\0');
}

// Copies text data into a form appropriate for disk
static void CopyTextData(char *pDst, const char *pSrc, int dstSize,
                         int srcSize) {
  const char *pSrcScan = pSrc;
  const char *pSrcEnd = pSrc + srcSize;
  char *pDstScan = pDst;

#ifdef _DEBUG
  char *pDstEnd = pDst + dstSize;
#endif

  for (; pSrcScan < pSrcEnd; pSrcScan++) {
    if (*pSrcScan == '\n') {
      *pDstScan = '\r';
      pDstScan++;
      *pDstScan = '\n';
      pDstScan++;
    } else {
      *pDstScan = *pSrcScan;
      pDstScan++;
    }
  }
  Assert(pSrcScan == pSrcEnd);
  Assert(pDstScan == pDstEnd);
}

// Purpose: Adds a new lump, or overwrites existing one
void ZipFile::AddBufferToZip(const char *relativename, void *data, int length,
                             bool bTextMode) {
  // Lower case only
  char name[512];
  strcpy_s(name, relativename);
  Q_strlower(name);

  int dstLength = length;
  if (bTextMode) {
    dstLength = GetLengthOfBinStringAsText((const char *)data, length);
  }

  // See if entry is in list already
  CZipEntry e;
  e.m_Name = name;
  int index = m_Files.Find(e);

  // If already existing, throw away old data and update data and length
  if (index != m_Files.InvalidIndex()) {
    CZipEntry *update = &m_Files[index];
    free(update->m_pData);

    if (bTextMode) {
      update->m_pData = malloc(dstLength);
      CopyTextData((char *)update->m_pData, (char *)data, dstLength, length);
      update->m_Length = dstLength;
    } else {
      update->m_pData = malloc(length);
      memcpy(update->m_pData, data, length);
      update->m_Length = length;
    }

    if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
      update->m_DiskCacheOffset = Win32File::FileTell(m_hDiskCacheWriteFile);
      Win32File::FileWrite(m_hDiskCacheWriteFile, update->m_pData,
                           update->m_Length);
      free(update->m_pData);
      update->m_pData = nullptr;
    }
  } else {
    // Create a new entry
    e.m_Length = dstLength;
    if (dstLength > 0) {
      if (bTextMode) {
        e.m_pData = malloc(dstLength);
        CopyTextData((char *)e.m_pData, (char *)data, dstLength, length);
      } else {
        e.m_pData = malloc(length);
        memcpy(e.m_pData, data, length);
      }

      if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
        e.m_DiskCacheOffset = Win32File::FileTell(m_hDiskCacheWriteFile);
        Win32File::FileWrite(m_hDiskCacheWriteFile, e.m_pData, e.m_Length);
        free(e.m_pData);
        e.m_pData = nullptr;
      }
    } else {
      e.m_pData = nullptr;
    }

    m_Files.Insert(e);
  }
}

// Reads a file from the zip
bool ZipFile::ReadFileFromZip(const char *pRelativeName, bool bTextMode,
                              CUtlBuffer &buf) {
  // Lower case only
  char pName[512];
  strcpy_s(pName, pRelativeName);
  Q_strlower(pName);

  // See if entry is in list already
  CZipEntry e;
  e.m_Name = pName;
  int nIndex = m_Files.Find(e);
  if (nIndex == m_Files.InvalidIndex()) {
    // not found
    return false;
  }

  CZipEntry *pEntry = &m_Files[nIndex];
  if (bTextMode) {
    buf.SetBufferType(true, false);
    ReadTextData((char *)pEntry->m_pData, pEntry->m_Length, buf);
  } else {
    buf.SetBufferType(false, false);
    buf.Put(pEntry->m_pData, pEntry->m_Length);
  }

  return true;
}

// Reads a file from the zip
bool ZipFile::ReadFileFromZip(HANDLE hZipFile, const char *pRelativeName,
                              bool bTextMode, CUtlBuffer &buf) {
  // Lower case only
  char pName[512];
  strcpy_s(pName, pRelativeName);
  Q_strlower(pName);

  // See if entry is in list already
  CZipEntry e;
  e.m_Name = pName;
  int nIndex = m_Files.Find(e);
  if (nIndex == m_Files.InvalidIndex()) {
    // not found
    return false;
  }

  CZipEntry *pEntry = &m_Files[nIndex];

  void *pData = malloc(pEntry->m_Length);
  Win32File::FileSeek(hZipFile, pEntry->m_SourceDiskOffset, FILE_BEGIN);
  if (!Win32File::FileRead(hZipFile, pData, pEntry->m_Length)) {
    free(pData);
    return false;
  }

  if (bTextMode) {
    buf.SetBufferType(true, false);
    ReadTextData((const char *)pData, pEntry->m_Length, buf);
  } else {
    buf.SetBufferType(false, false);
    buf.Put(pData, pEntry->m_Length);
  }

  free(pData);

  return true;
}

// Purpose: Check if a file already exists in the zip.
bool ZipFile::FileExistsInZip(const char *pRelativeName) {
  // Lower case only
  char pName[512];
  strcpy_s(pName, pRelativeName);
  Q_strlower(pName);

  // See if entry is in list already
  CZipEntry e;
  e.m_Name = pName;
  int nIndex = m_Files.Find(e);

  // If it is, then it exists in the pack!
  return nIndex != m_Files.InvalidIndex();
}

// Purpose: Adds a new file to the zip.
void ZipFile::AddFileToZip(const char *relativename, const char *fullpath) {
  FILE *fd;
  if (!fopen_s(&fd, fullpath, "rb")) {
    // Determine length
    fseek(fd, 0, SEEK_END);
    long size = ftell(fd);

    fseek(fd, 0, SEEK_SET);
    u8 *buf = (u8 *)malloc(size + 1);

    if (buf) {
      // Read data
      fread(buf, size, 1, fd);
    }

    fclose(fd);

    if (buf) {
      // Now add as a buffer
      AddBufferToZip(relativename, buf, size, false);
      free(buf);
    }
  }
}

// Purpose: Removes a file from the zip.
void ZipFile::RemoveFileFromZip(const char *relativename) {
  CZipEntry e;
  e.m_Name = relativename;
  int index = m_Files.Find(e);

  if (index != m_Files.InvalidIndex()) {
    CZipEntry update = m_Files[index];
    m_Files.Remove(update);
  }
}

//	Purpose: Calculates how many bytes should be added to the extra field
//  to push the start of the file data to the next aligned boundary
//  Output: Required padding size
unsigned short ZipFile::CalculatePadding(unsigned int filenameLen,
                                         unsigned int pos) {
  if (m_AlignmentSize == 0) {
    return 0;
  }

  unsigned int headerSize = sizeof(ZIP_LocalFileHeader) + filenameLen;
  return (unsigned short)(m_AlignmentSize -
                          ((pos + headerSize) % m_AlignmentSize));
}

// Purpose: Create the XZIP identifying comment string
// Output : Length
u16 ZipFile::MakeXZipCommentString(char *comment, size_t max_length) {
  char tmp[XZIP_COMMENT_LENGTH];
  memset(tmp, 0, sizeof(tmp));

  sprintf_s(tmp, "XZP%c %d", m_bCompatibleFormat ? '1' : '2', m_AlignmentSize);
  if (comment && max_length <= XZIP_COMMENT_LENGTH) {
    memcpy(comment, tmp, sizeof(tmp));
  }

  // expected fixed length
  return XZIP_COMMENT_LENGTH;
}

// Purpose: An XZIP has its configuration in the ascii comment
void ZipFile::ParseXZipCommentString(const char *pCommentString) {
  if (!_strnicmp(pCommentString, "XZP", 3)) {
    m_bCompatibleFormat = true;
    if (pCommentString[3] == '2') {
      m_bCompatibleFormat = false;
    }

    // parse out the alignement configuration
    if (!m_bForceAlignment) {
      m_AlignmentSize = 0;
      sscanf_s(pCommentString + 4, "%u", &m_AlignmentSize);
      if (!IsPowerOfTwo(m_AlignmentSize)) {
        m_AlignmentSize = 0;
      }
    }
  }
}

// Purpose: Calculate the exact size of zip file, with headers and padding
// Output : int
unsigned int ZipFile::CalculateSize() {
  unsigned int size = 0;
  unsigned int dirHeaders = 0;
  for (int i = m_Files.FirstInorder(); i != m_Files.InvalidIndex();
       i = m_Files.NextInorder(i)) {
    CZipEntry *e = &m_Files[i];

    if (e->m_Length == 0) continue;

    // local file header
    size += sizeof(ZIP_LocalFileHeader);
    size += strlen(e->m_Name.String());

    // every file has a directory header that duplicates the filename
    dirHeaders += sizeof(ZIP_FileHeader) + strlen(e->m_Name.String());

    // calculate padding
    if (m_AlignmentSize != 0) {
      // round up to next boundary
      unsigned int nextBoundary =
          (size + m_AlignmentSize) & ~(m_AlignmentSize - 1);

      // the directory header also duplicates the padding
      dirHeaders += nextBoundary - size;

      size = nextBoundary;
    }

    // data size
    size += e->m_Length;
  }

  size += dirHeaders;

  // All processed zip files will have a comment string
  size += sizeof(ZIP_EndOfCentralDirRecord) + MakeXZipCommentString(nullptr, 0);

  return size;
}

// Purpose: Print a directory of files in the zip

void ZipFile::PrintDirectory() {
  for (int i = m_Files.FirstInorder(); i != m_Files.InvalidIndex();
       i = m_Files.NextInorder(i)) {
    CZipEntry *e = &m_Files[i];

    Msg("%s\n", e->m_Name.String());
  }
}

// Purpose: Iterate through directory

int ZipFile::GetNextFilename(int id, char *pBuffer, int bufferSize,
                             int &fileSize) {
  if (id == -1) {
    id = m_Files.FirstInorder();
  } else {
    id = m_Files.NextInorder(id);
  }
  if (id == m_Files.InvalidIndex()) {
    // list is empty
    return -1;
  }

  CZipEntry *e = &m_Files[id];

  strcpy_s(pBuffer, bufferSize, e->m_Name.String());
  fileSize = e->m_Length;

  return id;
}

// Purpose: Store data out to disk

void ZipFile::SaveToDisk(FILE *fout) {
  FileWriteStream stream(fout);
  SaveDirectory(stream);
}

void ZipFile::SaveToDisk(HANDLE hOutFile) {
  FileWriteStream stream(hOutFile);
  SaveDirectory(stream);
}

// Purpose: Store data out to a CUtlBuffer

void ZipFile::SaveToBuffer(CUtlBuffer &buf) {
  BufferWriteStream stream(buf);
  SaveDirectory(stream);
}

// Purpose: Store data back out to a stream (could be CUtlBuffer or filestream)
void ZipFile::SaveDirectory(IWriteStream &stream) {
  void *padding_buffer = nullptr;
  if (m_AlignmentSize) {
    // get a temp buffer for all padding work
    padding_buffer = malloc(m_AlignmentSize);
    memset(padding_buffer, 0x00, m_AlignmentSize);
  }

  if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(m_hDiskCacheWriteFile);
  }

  for (int i = m_Files.FirstInorder(); i != m_Files.InvalidIndex();
       i = m_Files.NextInorder(i)) {
    CZipEntry *e = &m_Files[i];
    Assert(e);

    // Fix up the offset
    e->m_ZipOffset = stream.Tell();

    if (e->m_Length > 0 && (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE)) {
      // get the data back from the write cache
      e->m_pData = malloc(e->m_Length);

      if (e->m_pData) {
        Win32File::FileSeek(m_hDiskCacheWriteFile, e->m_DiskCacheOffset,
                            FILE_BEGIN);
        Win32File::FileRead(m_hDiskCacheWriteFile, e->m_pData, e->m_Length);
      }
    }

    if (e->m_Length > 0 && e->m_pData != nullptr) {
      ZIP_LocalFileHeader hdr{0};
      hdr.signature = PKID(3, 4);
      // This is the version that the winzip that I have writes.
      hdr.versionNeededToExtract = 10;
      hdr.flags = 0;
      hdr.compressionMethod = 0;  // NO COMPRESSION!
      hdr.lastModifiedTime = 0;
      hdr.lastModifiedDate = 0;

      CRC32_Init(&e->m_ZipCRC);
      CRC32_ProcessBuffer(&e->m_ZipCRC, e->m_pData, e->m_Length);
      CRC32_Final(&e->m_ZipCRC);
      hdr.crc32 = e->m_ZipCRC;

      const char *pFilename = e->m_Name.String();
      hdr.compressedSize = e->m_Length;
      hdr.uncompressedSize = e->m_Length;

      Assert(strlen(pFilename) <= USHRT_MAX);

      hdr.fileNameLength = (u16)strlen(pFilename);
      hdr.extraFieldLength =
          CalculatePadding(hdr.fileNameLength, e->m_ZipOffset);
      u16 extraFieldLength = hdr.extraFieldLength;

      // Swap header in place
      m_Swap.SwapFieldsToTargetEndian(&hdr);
      stream.Put(&hdr, sizeof(hdr));
      stream.Put(pFilename, strlen(pFilename));
      stream.Put(padding_buffer, extraFieldLength);
      stream.Put(e->m_pData, e->m_Length);

      if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
        free(e->m_pData);

        // temp hackery for the logic below to succeed
        e->m_pData = (void *)0xFFFFFFFF;
      }
    }
  }

  if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
    Win32File::FileSeek(m_hDiskCacheWriteFile, 0, FILE_END);
  }

  usize centralDirStart = stream.Tell();
  if (m_AlignmentSize) {
    // align the central directory starting position
    usize newDirStart = AlignValue(centralDirStart, m_AlignmentSize);
    usize padLength = newDirStart - centralDirStart;

    if (padLength) {
      stream.Put(padding_buffer, padLength);
      centralDirStart = newDirStart;
    }
  }

  int realNumFiles = 0;
  for (int i = m_Files.FirstInorder(); i != m_Files.InvalidIndex();
       i = m_Files.NextInorder(i)) {
    CZipEntry *e = &m_Files[i];
    Assert(e);

    if (e->m_Length > 0 && e->m_pData != nullptr) {
      ZIP_FileHeader hdr = {0};
      hdr.signature = PKID(1, 2);
      hdr.versionMadeBy =
          20;  // This is the version that the winzip that I have writes.
      hdr.versionNeededToExtract =
          10;  // This is the version that the winzip that I have writes.
      hdr.flags = 0;
      hdr.compressionMethod = 0;
      hdr.lastModifiedTime = 0;
      hdr.lastModifiedDate = 0;
      hdr.crc32 = e->m_ZipCRC;

      hdr.compressedSize = e->m_Length;
      hdr.uncompressedSize = e->m_Length;

      Assert(strlen(e->m_Name.String()) <= USHRT_MAX);

      hdr.fileNameLength = (unsigned short)strlen(e->m_Name.String());
      hdr.extraFieldLength =
          CalculatePadding(hdr.fileNameLength, e->m_ZipOffset);
      hdr.fileCommentLength = 0;
      hdr.diskNumberStart = 0;
      hdr.internalFileAttribs = 0;
      hdr.externalFileAttribs = 0;  // This is usually something, but zero is OK
                                    // as if the input came from stdin
      hdr.relativeOffsetOfLocalHeader = e->m_ZipOffset;
      u16 extraFieldLength = hdr.extraFieldLength;

      // Swap the header in place
      m_Swap.SwapFieldsToTargetEndian(&hdr);

      stream.Put(&hdr, sizeof(hdr));
      stream.Put(e->m_Name.String(), strlen(e->m_Name.String()));

      if (m_bCompatibleFormat) {
        stream.Put(padding_buffer, extraFieldLength);
      }

      realNumFiles++;

      if (m_hDiskCacheWriteFile != INVALID_HANDLE_VALUE) {
        // clear out temp hackery
        e->m_pData = nullptr;
      }
    }
  }

  usize centralDirEnd = stream.Tell();
  if (m_AlignmentSize) {
    // align the central directory starting position
    usize newDirEnd = AlignValue(centralDirEnd, m_AlignmentSize);
    usize padLength = newDirEnd - centralDirEnd;

    if (padLength) {
      stream.Put(padding_buffer, padLength);
      centralDirEnd = newDirEnd;
    }
  }

  ZIP_EndOfCentralDirRecord rec{0};
  rec.signature = PKID(5, 6);
  rec.numberOfThisDisk = 0;
  rec.numberOfTheDiskWithStartOfCentralDirectory = 0;
  rec.nCentralDirectoryEntries_ThisDisk = realNumFiles;
  rec.nCentralDirectoryEntries_Total = realNumFiles;
  rec.centralDirectorySize = centralDirEnd - centralDirStart;
  rec.startOfCentralDirOffset = centralDirStart;

  char commentString[128];
  u16 comment_size =
      MakeXZipCommentString(commentString, SOURCE_ARRAYSIZE(commentString));
  rec.commentLength = comment_size;

  // Swap the header in place
  m_Swap.SwapFieldsToTargetEndian(&rec);
  stream.Put(&rec, sizeof(rec));
  stream.Put(commentString, comment_size);

  free(padding_buffer);
}

class CZip : public IZip {
 public:
  CZip(const char *pDiskCacheWritePath, bool bSortByName)
      : m_ZipFile(pDiskCacheWritePath, bSortByName) {
    m_ZipFile.Reset();
  }
  virtual ~CZip() {}

  virtual void Reset() { m_ZipFile.Reset(); }

  // Add a single file to a zip - maintains the zip's previous alignment state
  virtual void AddFileToZip(const char *relativename, const char *fullpath) {
    m_ZipFile.AddFileToZip(relativename, fullpath);
  }

  // Whether a file is contained in a zip - maintains alignment
  virtual bool FileExistsInZip(const char *pRelativeName) {
    return m_ZipFile.FileExistsInZip(pRelativeName);
  }

  // Reads a file from the zip - maintains alignement
  virtual bool ReadFileFromZip(const char *pRelativeName, bool bTextMode,
                               CUtlBuffer &buf) {
    return m_ZipFile.ReadFileFromZip(pRelativeName, bTextMode, buf);
  }
  virtual bool ReadFileFromZip(HANDLE hZipFile, const char *relativename,
                               bool bTextMode, CUtlBuffer &buf) {
    return m_ZipFile.ReadFileFromZip(hZipFile, relativename, bTextMode, buf);
  }

  // Removes a single file from the zip - maintains alignment
  virtual void RemoveFileFromZip(const char *relativename) {
    m_ZipFile.RemoveFileFromZip(relativename);
  }

  // Gets next filename in zip, for walking the directory - maintains alignment
  virtual int GetNextFilename(int id, char *pBuffer, int bufferSize,
                              int &fileSize) {
    return m_ZipFile.GetNextFilename(id, pBuffer, bufferSize, fileSize);
  }

  // Prints the zip's contents - maintains alignment
  virtual void PrintDirectory() { m_ZipFile.PrintDirectory(); }

  // Estimate the size of the Zip (including header, padding, etc.)
  virtual unsigned int EstimateSize() { return m_ZipFile.CalculateSize(); }

  // Add buffer to zip as a file with given name - uses current alignment size,
  // default 0 (no alignment)
  // Add buffer to zip as a file with given name
  virtual void AddBufferToZip(const char *relativename, void *data, int length,
                              bool bTextMode) {
    m_ZipFile.AddBufferToZip(relativename, data, length, bTextMode);
  }

  // Writes out zip file to a buffer - uses current alignment size
  // (set by file's previous alignment, or a call to ForceAlignment)
  virtual void SaveToBuffer(CUtlBuffer &outbuf) {
    m_ZipFile.SaveToBuffer(outbuf);
  }

  // Writes out zip file to a filestream - uses current alignment size
  // (set by file's previous alignment, or a call to ForceAlignment)
  virtual void SaveToDisk(FILE *fout) { m_ZipFile.SaveToDisk(fout); }
  virtual void SaveToDisk(HANDLE hOutFile) { m_ZipFile.SaveToDisk(hOutFile); }

  // Reads a zip file from a buffer into memory - sets current alignment size to
  // the file's alignment size, unless overridden by a ForceAlignment call)
  virtual void ParseFromBuffer(void *buffer, int bufferlength) {
    m_ZipFile.Reset();
    m_ZipFile.ParseFromBuffer(buffer, bufferlength);
  }
  virtual HANDLE ParseFromDisk(const char *pFilename) {
    m_ZipFile.Reset();
    return m_ZipFile.ParseFromDisk(pFilename);
  }

  // Forces a specific alignment size for all subsequent file operations,
  // overriding files' previous alignment size. Return to using files'
  // individual alignment sizes by passing FALSE.
  virtual void ForceAlignment(bool aligned, bool bCompatibleFormat,
                              unsigned int alignmentSize) {
    m_ZipFile.ForceAlignment(aligned, bCompatibleFormat, alignmentSize);
  }

  // Sets the endianess of the zip
  virtual void SetBigEndian(bool bigEndian) {
    m_ZipFile.SetBigEndian(bigEndian);
  }
  virtual void ActivateByteSwapping(bool bActivate) {
    m_ZipFile.ActivateByteSwapping(bActivate);
  }

  virtual unsigned int GetAlignment() { return m_ZipFile.GetAlignment(); }

 private:
  ZipFile m_ZipFile;
};

static CUtlLinkedList<CZip *> g_ZipUtils;

IZip *IZip::CreateZip(const char *pDiskCacheWritePath, bool bSortByName) {
  CZip *pZip = new CZip(pDiskCacheWritePath, bSortByName);
  g_ZipUtils.AddToTail(pZip);

  return pZip;
}

void IZip::ReleaseZip(IZip *pZip) {
  g_ZipUtils.FindAndRemove((CZip *)pZip);

  delete ((CZip *)pZip);
}

#endif  // SWDS
