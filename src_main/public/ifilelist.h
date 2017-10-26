// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef IFILELIST_H
#define IFILELIST_H

// This class represents a group of files. Internally, it can represent whole
// folders of files that are in or out of the group. So you can't iterate the
// list, but you can ask the class if a particular filename is in the list.
class IFileList {
 public:
  virtual bool IsFileInList(const char *pFilename) = 0;
  virtual void Release() = 0;
};

#endif  // IFILELIST_H
