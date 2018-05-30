// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Test for debug section.
//
// Adapted from PEDUMP, AUTHOR:  Matt Pietrek - 1993

#include <windows.h>
#include <cstdio>
#include "common.h"
#include "tier1/strtools.h"

bool HasSection(PIMAGE_SECTION_HEADER section, int numSections,
                const char *pSectionName) {
  for (int i = 0; i < numSections; i++) {
    if (!_strnicmp((char *)section[i].Name, pSectionName, 8)) return true;
  }

  return false;
}

void TestExeFile(const char *pFilename, PIMAGE_DOS_HEADER dosHeader) {
  PIMAGE_NT_HEADERS pNTHeader =
      MakePtr(PIMAGE_NT_HEADERS, dosHeader, dosHeader->e_lfanew);

  // First, verify that the e_lfanew field gave us a reasonable
  // pointer, then verify the PE signature.
  if (IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) ||
      pNTHeader->Signature != IMAGE_NT_SIGNATURE) {
    fprintf(stderr, "Unhandled exe type, or invalid .exe (%s).\n", pFilename);
    return;
  }

  if (HasSection((PIMAGE_SECTION_HEADER)(pNTHeader + 1),
                 pNTHeader->FileHeader.NumberOfSections, "ValveDBG")) {
    printf("%s is a debug build.\n", pFilename);
  }
}

// Open up a file, memory map it, and call the appropriate dumping routine
void TestFile(const char *file_name) {
  HANDLE hFile = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

  if (hFile == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Couldn't open file %s with CreateFile().\n", file_name);
    return;
  }

  HANDLE hFileMapping =
      CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (hFileMapping == nullptr) {
    CloseHandle(hFile);
    fprintf(stderr, "Couldn't open file %s mapping with CreateFileMapping().\n",
            file_name);
    return;
  }

  LPVOID lpFileBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
  if (lpFileBase == 0) {
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
    fprintf(stderr, "Couldn't map view of file %s with MapViewOfFile().\n",
            file_name);
    return;
  }

  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)lpFileBase;
  if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
    TestExeFile(file_name, dosHeader);
  } else {
    fprintf(stderr, "Unrecognized file %s format, magic %hu.\n", file_name,
            dosHeader->e_magic);
  }

  UnmapViewOfFile(lpFileBase);
  CloseHandle(hFileMapping);
  CloseHandle(hFile);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: test_binaries <FILENAME>\n");
    return EXIT_FAILURE;
  }

  char fileName[2048], dir[2048];

  if (!Q_ExtractFilePath(argv[1], dir, sizeof(dir))) {
    strcpy_s(dir, "");
  } else {
    Q_FixSlashes(dir, '/');

    std::size_t len = strlen(dir);

    if (len && dir[len - 1] != '/') strcat_s(dir, "/");
  }

  WIN32_FIND_DATA findData;
  HANDLE hFind = FindFirstFile(argv[1], &findData);

  if (hFind == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Can't find file %s.\n", argv[1]);
  } else {
    do {
      sprintf_s(fileName, "%s%s", dir, findData.cFileName);
      TestFile(fileName);
    } while (FindNextFile(hFind, &findData));

    FindClose(hFind);
  }

  return EXIT_SUCCESS;
}
