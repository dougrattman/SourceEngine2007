// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef STDSTRING_H
#define STDSTRING_H

#include <string>

#include "isaverestore.h"

class CStdStringSaveRestoreOps : public CDefSaveRestoreOps {
 public:
  enum {
    MAX_SAVE_LEN = 4096,
  };

  // save data type interface
  virtual void Save(const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave) {
    std::string *pString = (std::string *)fieldInfo.pField;
    Assert(pString->length() < MAX_SAVE_LEN - 1);
    if (pString->length() < MAX_SAVE_LEN - 1)
      pSave->WriteString(pString->c_str());
    else
      pSave->WriteString("<<invalid>>");
  }

  virtual void Restore(const SaveRestoreFieldInfo_t &fieldInfo,
                       IRestore *pRestore) {
    std::string *pString = (std::string *)fieldInfo.pField;
    char szString[MAX_SAVE_LEN];
    pRestore->ReadString(szString, sizeof(szString), 0);
    szString[MAX_SAVE_LEN - 1] = 0;
    pString->assign(szString);
  }

  virtual void MakeEmpty(const SaveRestoreFieldInfo_t &fieldInfo) {
    std::string *pString = (std::string *)fieldInfo.pField;
    pString->erase();
  }

  virtual bool IsEmpty(const SaveRestoreFieldInfo_t &fieldInfo) {
    std::string *pString = (std::string *)fieldInfo.pField;
    return pString->empty();
  }
};

//-------------------------------------

inline ISaveRestoreOps *GetStdStringDataOps() {
  static CStdStringSaveRestoreOps ops;
  return &ops;
}

//-------------------------------------

#define DEFINE_STDSTRING(name)                                     \
  {                                                                \
    FIELD_CUSTOM, #name, {offsetof(classNameTypedef, name), 0}, 1, \
        FTYPEDESC_SAVE, NULL, GetStdStringDataOps(), NULL          \
  }

#endif  // STDSTRING_H
