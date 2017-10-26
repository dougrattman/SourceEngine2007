// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "snd_io.h"

#include "filesystem.h"
#include "filesystem_engine.h"
#include "tier1/strtools.h"
#include "tier2/riff.h"

#include "tier0/memdbgon.h"

// Purpose: Implements Audio IO on the engine's COMMON filesystem
class COM_IOReadBinary : public IFileReadBinary {
 public:
  int open(const char *pFileName) override;
  int read(void *pOutput, int size, int file) override;
  void seek(int file, int pos) override;
  unsigned int tell(int file) override;
  unsigned int size(int file) override;
  void close(int file) override;
};

// prepend sound/ to the filename -- all sounds are loaded from the sound/
// directory
int COM_IOReadBinary::open(const char *pFileName) {
  char namebuffer[512];
  Q_strncpy(namebuffer, "sound", ARRAYSIZE(namebuffer));

  // the server is sending back sound names with slashes in front...
  if (pFileName[0] != '/' && pFileName[0] != '\\') {
    Q_strncat(namebuffer, "/", ARRAYSIZE(namebuffer), COPY_ALL_CHARACTERS);
  }

  Q_strncat(namebuffer, pFileName, ARRAYSIZE(namebuffer), COPY_ALL_CHARACTERS);

  FileHandle_t hFile = g_pFileSystem->Open(namebuffer, "rb", "GAME");

  return (int)hFile;
}

int COM_IOReadBinary::read(void *pOutput, int size, int file) {
  if (!file) return 0;

  return g_pFileSystem->Read(pOutput, size, (FileHandle_t)file);
}

void COM_IOReadBinary::seek(int file, int pos) {
  if (!file) return;

  g_pFileSystem->Seek((FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD);
}

unsigned int COM_IOReadBinary::tell(int file) {
  if (!file) return 0;

  return g_pFileSystem->Tell((FileHandle_t)file);
}

unsigned int COM_IOReadBinary::size(int file) {
  if (!file) return 0;

  return g_pFileSystem->Size((FileHandle_t)file);
}

void COM_IOReadBinary::close(int file) {
  if (!file) return;

  g_pFileSystem->Close((FileHandle_t)file);
}

static COM_IOReadBinary io;
IFileReadBinary *g_pSndIO = &io;
