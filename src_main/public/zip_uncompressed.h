// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ZIP_UNCOMPRESSED_H
#define ZIP_UNCOMPRESSED_H

#include "base/include/base_types.h"
#include "datamap.h"

#define PKID(a, b) (((b) << 24) | ((a) << 16) | ('K' << 8) | 'P')

#pragma pack(push, 1)

struct ZIP_EndOfCentralDirRecord {
  DECLARE_BYTESWAP_DATADESC();
  u32 signature;                                   // 4 bytes PK56
  u16 numberOfThisDisk;                            // 2 bytes
  u16 numberOfTheDiskWithStartOfCentralDirectory;  // 2 bytes
  u16 nCentralDirectoryEntries_ThisDisk;           // 2 bytes
  u16 nCentralDirectoryEntries_Total;              // 2 bytes
  u32 centralDirectorySize;                        // 4 bytes
  u32 startOfCentralDirOffset;                     // 4 bytes
  u16 commentLength;                               // 2 bytes
  // zip file comment follows
};

struct ZIP_FileHeader {
  DECLARE_BYTESWAP_DATADESC();
  u32 signature;                    //  4 bytes PK12
  u16 versionMadeBy;                // version made by 2 bytes
  u16 versionNeededToExtract;       // version needed to extract 2 bytes
  u16 flags;                        // general purpose bit flag 2 bytes
  u16 compressionMethod;            // compression method 2 bytes
  u16 lastModifiedTime;             // last mod file time 2 bytes
  u16 lastModifiedDate;             // last mod file date 2 bytes
  u32 crc32;                        // crc-32 4 bytes
  u32 compressedSize;               // compressed size 4 bytes
  u32 uncompressedSize;             // uncompressed size 4 bytes
  u16 fileNameLength;               // file name length 2 bytes
  u16 extraFieldLength;             // extra field length 2 bytes
  u16 fileCommentLength;            // file comment length 2 bytes
  u16 diskNumberStart;              // disk number start 2 bytes
  u16 internalFileAttribs;          // internal file attributes 2 bytes
  u32 externalFileAttribs;          // external file attributes 4 bytes
  u32 relativeOffsetOfLocalHeader;  // relative offset of local header 4 bytes
                                    // file name (variable size)
                                    // extra field (variable size)
                                    // file comment (variable size)
};

struct ZIP_LocalFileHeader {
  DECLARE_BYTESWAP_DATADESC();
  u32 signature;               // local file header signature 4 bytes PK34
  u16 versionNeededToExtract;  // version needed to extract 2 bytes
  u16 flags;                   // general purpose bit flag 2 bytes
  u16 compressionMethod;       // compression method 2 bytes
  u16 lastModifiedTime;        // last mod file time 2 bytes
  u16 lastModifiedDate;        // last mod file date 2 bytes
  u32 crc32;                   // crc-32 4 bytes
  u32 compressedSize;          // compressed size 4 bytes
  u32 uncompressedSize;        // uncompressed size 4 bytes
  u16 fileNameLength;          // file name length 2 bytes
  u16 extraFieldLength;        // extra field length 2 bytes
                               // file name (variable size)
                               // extra field (variable size)
};

//	Valve Non standard Extension, Preload Section
//	An optional first file in an aligned zip that can be loaded into ram and
//	used by the FileSystem to supply header data rather than disk.
//	Is is an optimization to prevent the large of amount of small I/O
//  performed by the map loading process.

#define PRELOAD_SECTION_NAME "__preload_section.pre"
#define PRELOAD_HDR_VERSION 3
#define XZIP_COMMENT_LENGTH 32
#define INVALID_PRELOAD_ENTRY ((u16)-1)

struct ZIP_PreloadHeader {
  DECLARE_BYTESWAP_DATADESC();
  u32 Version;                  // VERSION
  u32 DirectoryEntries;         // Number of zip directory entries.
  u32 PreloadDirectoryEntries;  // Number of preloaded directory
                                // entries (equal or less than the zip
                                // dir).
  u32 Alignment;                // File alignment of the zip
};

struct ZIP_PreloadDirectoryEntry {
  DECLARE_BYTESWAP_DATADESC();
  u32 Length;      // Length of the file's preload data in bytes
  u32 DataOffset;  // Offset the file data in the .zip, relative to the
                   // logical beginning of the preload file.
};

struct ZIP_PreloadRemapTable {
  DECLARE_BYTESWAP_DATADESC();
  u16 PreloadIndex;  // Index into preload directory, entry marked
                     // invalid if no preload entry present
};

#pragma pack(pop)

#endif  // ZIP_UNCOMPRESSED_H
