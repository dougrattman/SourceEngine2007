// Copyright © 1996-2006, Valve Corporation, All rights reserved.
//
// Purpose: Command sink interface implementation.

#ifndef CMDSINK_H
#define CMDSINK_H

#include <cstdio>
#include "tier1/utlbuffer.h"

namespace CmdSink {
// Interface to give back command execution results.
struct IResponse {
  virtual ~IResponse() {}
  virtual void Release() { delete this; }

  // Returns whether the command succeeded
  virtual bool Succeeded() = 0;

  // If the command succeeded returns the result buffer length, otherwise zero
  virtual size_t GetResultBufferLen() = 0;
  // If the command succeeded returns the result buffer base pointer, otherwise
  // NULL
  virtual const void *GetResultBuffer() = 0;

  // Returns a zero-terminated string of messages reported during command
  // execution, or NULL if nothing was reported
  virtual const char *GetListing() = 0;
};

/*
Response implementation when the result should appear in
one file and the listing should appear in another file.
*/
class CResponseFiles : public IResponse {
 public:
  explicit CResponseFiles(char const *szFileResult, char const *szFileListing);
  ~CResponseFiles();

 public:
  // Returns whether the command succeeded
  virtual bool Succeeded();

  // If the command succeeded returns the result buffer length, otherwise zero
  virtual size_t GetResultBufferLen();
  // If the command succeeded returns the result buffer base pointer, otherwise
  // NULL
  virtual const void *GetResultBuffer();

  // Returns a zero-terminated string of messages reported during command
  // execution
  virtual const char *GetListing();

 protected:
  void OpenResultFile();   //!< Opens the result file if not open yet
  void ReadResultFile();   //!< Reads the result buffer if not read yet
  void ReadListingFile();  //!< Reads the listing buffer if not read yet

 protected:
  char m_szFileResult[MAX_PATH];   //!< Name of the result file
  char m_szFileListing[MAX_PATH];  //!< Name of the listing file

  FILE *m_fResult;   //!< Result file (NULL if not open)
  FILE *m_fListing;  //!< Listing file (NULL if not open)

  CUtlBuffer m_bufResult;  //!< Buffer holding the result data
  size_t m_lenResult;      //!< Result data length (0 if result not read yet)
  const void
      *m_dataResult;  //!< Data buffer pointer (NULL if result not read yet)

  CUtlBuffer m_bufListing;  //!< Buffer holding the listing
  const char *
      m_dataListing;  //!< Listing buffer pointer (NULL if listing not read yet)
};

/*
Response implementation when the result is a generic error.
*/
class CResponseError : public IResponse {
 public:
  explicit CResponseError() {}
  ~CResponseError() {}

 public:
  virtual bool Succeeded() { return false; }

  virtual size_t GetResultBufferLen() { return 0; }
  virtual const void *GetResultBuffer() { return nullptr; }

  virtual const char *GetListing() { return nullptr; }
};
};  // namespace CmdSink

#endif  // CMDSINK_H
