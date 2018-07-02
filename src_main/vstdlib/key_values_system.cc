// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "vstdlib/IKeyValuesSystem.h"

#include "tier0/include/threadtools.h"
#include "tier1/keyvalues.h"
#include "tier1/mempool.h"
#include "tier1/memstack.h"
#include "tier1/utlsymbol.h"

#include "tier0/include/memdbgon.h"

// no need to pool if using tier0 small block heap
#ifdef NO_SBH
#define KEYVALUES_USE_POOL 1
#endif

#ifdef KEYVALUES_USE_POOL
static void KVLeak(char const *fmt, ...) {
  va_list argptr;
  char data[1024];

  va_start(argptr, fmt);
  Q_vsnprintf(data, sizeof(data), fmt, argptr);
  va_end(argptr);

  Msg(data);
}
#endif

// Central storage point for KeyValues memory and symbols.
class CKeyValuesSystem : public IKeyValuesSystem {
 public:
  CKeyValuesSystem()
      : m_HashItemMemPool{sizeof(HashItem), 64, CMemoryPool::GROW_FAST,
                          "CKeyValuesSystem::m_HashItemMemPool"},
        m_KeyValuesTrackingList{0, 0, MemoryLeakTrackerLessFunc} {
    // initialize hash table
    m_HashTable.AddMultipleToTail(2047);

    for (int i = 0; i < m_HashTable.Count(); i++) {
      m_HashTable[i].stringIndex = 0;
      m_HashTable[i].next = nullptr;
    }

    m_Strings.Init(4 * 1024 * 1024, 64 * 1024, 0, 4);
    char *pszEmpty = ((char *)m_Strings.Alloc(1));
    *pszEmpty = 0;

#ifdef KEYVALUES_USE_POOL
    m_pMemPool = nullptr;
#endif
    m_iMaxKeyValuesSize = sizeof(KeyValues);
  }
  ~CKeyValuesSystem() {
#ifdef KEYVALUES_USE_POOL
#ifdef _DEBUG
    // display any memory leaks
    if (m_pMemPool && m_pMemPool->Count() > 0) {
      DevMsg("Leaked KeyValues blocks: %d\n", m_pMemPool->Count());
    }

    // iterate all the existing keyvalues displaying their names
    for (int i = 0; i < m_KeyValuesTrackingList.MaxElement(); i++) {
      if (m_KeyValuesTrackingList.IsValidIndex(i)) {
        DevMsg("\tleaked KeyValues(%s)\n",
               &m_Strings[m_KeyValuesTrackingList[i].nameIndex]);
      }
    }
#endif

    delete m_pMemPool;
#endif
  }

  // Registers the size of the KeyValues in the specified instance so it can
  // build a properly sized memory pool for the KeyValues objects the sizes will
  // usually never differ but this is for versioning safety.
  void RegisterSizeofKeyValues(usize size) override {
    if (size > m_iMaxKeyValuesSize) {
      m_iMaxKeyValuesSize = size;
    }
  }

  // Allocates a KeyValues object from the shared mempool.
  void *AllocKeyValuesMemory(usize size) override {
#ifdef KEYVALUES_USE_POOL
    // allocate, if we don't have one yet
    if (!m_pMemPool) {
      m_pMemPool =
          new CMemoryPool(m_iMaxKeyValuesSize, 1024, CMemoryPool::GROW_FAST,
                          "CKeyValuesSystem::m_pMemPool");
      m_pMemPool->SetErrorReportFunc(KVLeak);
    }

    return m_pMemPool->Alloc(size);
#else
    return malloc(size);
#endif
  }

  // Frees a KeyValues object from the shared mempool.
  void FreeKeyValuesMemory(void *pMem) override {
#ifdef KEYVALUES_USE_POOL
    m_pMemPool->Free(pMem);
#else
    heap_free(pMem);
#endif
  }

  // Symbol table access (used for key names).
  HKeySymbol GetSymbolForString(const char *name, bool should_create) override {
    if (!name) return -1;

    size_t size{strlen(name) + 1};

    AUTO_LOCK(m_mutex);

    int hash = CaseInsensitiveHash(name, m_HashTable.Count());
    HashItem *item = &m_HashTable[hash];

    while (true) {
      if (!_stricmp(name, (char *)m_Strings.GetBase() + item->stringIndex)) {
        return (HKeySymbol)item->stringIndex;
      }

      if (item->next == nullptr) {
        // not found
        if (!should_create) return -1;

        // we're not in the table
        if (item->stringIndex != 0) {
          // first item is used, an new item
          item->next =
              (HashItem *)m_HashItemMemPool.Alloc(sizeof(HashItem));
          item = item->next;
        }

        // build up the new item
        item->next = nullptr;

        char *the_string = (char *)m_Strings.Alloc(size);  //-V814
        if (!the_string) {
          Error("KeyValuesSystem: Can't alloc string of size %zu.", size);
          return -1;
        }

        item->stringIndex = the_string - (char *)m_Strings.GetBase();
        strcpy_s(the_string, size, name);

        return (HKeySymbol)item->stringIndex;
      }

      item = item->next;
    }

    // shouldn't be able to get here
    Assert(0);

    return -1;
  }

  // Symbol table access.
  const char *GetStringForSymbol(HKeySymbol symbol) override {
    if (symbol == -1) return "";

    return ((char *)m_Strings.GetBase() + (size_t)symbol);
  }

  // Adds KeyValues record into global list so we can track memory leaks.
  void AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name) override {
#ifdef _DEBUG
    // only track the memory leaks in debug builds
    MemoryLeakTracker item = {name, pMem};
    m_KeyValuesTrackingList.Insert(item);
#endif
  }
  // Used to track memory leaks.
  void RemoveKeyValuesFromMemoryLeakList(void *pMem) override {
#ifdef _DEBUG
    // only track the memory leaks in debug builds
    MemoryLeakTracker item = {0, pMem};
    int index = m_KeyValuesTrackingList.Find(item);
    m_KeyValuesTrackingList.RemoveAt(index);
#endif
  }

 private:
  struct HashItem {
    int stringIndex;
    HashItem *next;
  };

  struct MemoryLeakTracker {
    int nameIndex;
    void *pMem;
  };

#ifdef KEYVALUES_USE_POOL
  CMemoryPool *m_pMemPool;
#endif
  usize m_iMaxKeyValuesSize;

  // string hash table
  CMemoryStack m_Strings;
  CMemoryPool m_HashItemMemPool;

  CUtlVector<HashItem> m_HashTable;
  CUtlRBTree<MemoryLeakTracker, int> m_KeyValuesTrackingList;

  CThreadFastMutex m_mutex;

  // Generates a simple hash value for a string.
  int CaseInsensitiveHash(const char *value, int bound) {
    u32 hash{0};

    for (; *value != 0; value++) {
      if (*value >= 'A' && *value <= 'Z') {
        hash = (hash << 1) + (*value - 'A' + 'a');
      } else {
        hash = (hash << 1) + *value;
      }
    }

    return hash % bound;
  }

  static bool MemoryLeakTrackerLessFunc(const MemoryLeakTracker &lhs,
                                        const MemoryLeakTracker &rhs) {
    return lhs.pMem < rhs.pMem;
  }
};

// Instance singleton and expose interface to rest of code.
IKeyValuesSystem *KeyValuesSystem() {
  static CKeyValuesSystem key_values_system;
  return &key_values_system;
}
