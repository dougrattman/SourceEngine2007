// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Tools for correctly implementing & handling reference counted
// objects.

#ifndef SOURCE_TIER1_REFCOUNT_H_
#define SOURCE_TIER1_REFCOUNT_H_

#include "base/include/compiler_specific.h"
#include "tier0/include/dbg.h"
#include "tier0/include/threadtools.h"

// Implement a standard reference counted interface. Use of this is optional
// insofar as all the concrete tools only require at compile time that the
// function signatures match.
the_interface IRefCounted {
 public:
  virtual int AddRef() = 0;
  virtual int Release() = 0;
};

// Release a pointer and mark it nullptr.
template <typename TRefCountedItemPtr>
inline int SafeRelease(TRefCountedItemPtr &ref) {
  // Use funny syntax so that this works on "auto pointers".
  auto *ref_ptr = &ref;

  if (*ref_ptr) {
    int result = (*ref_ptr)->Release();
    *ref_ptr = nullptr;

    return result;
  }

  return 0;
}

// Maintain a reference across a scope.
template <typename T = IRefCounted>
class CAutoRef {
 public:
  CAutoRef(T *ref) : ref_(ref) {
    if (ref_) ref_->AddRef();
  }

  ~CAutoRef() {
    if (ref_) ref_->Release();
  }

 private:
  T *const ref_;
};

// Do a an inline AddRef then return the pointer, useful when returning an
// object from a function.
#define RetAddRef(p) ((p)->AddRef(), (p))
#define InlineAddRef(p) ((p)->AddRef(), (p))

// A class to both hold a pointer to an object and its reference.  Base exists
// to support other cleanup models.
template <typename T>
class CBaseAutoPtr {
 public:
  CBaseAutoPtr() : m_pObject(0) {}
  CBaseAutoPtr(T *pFrom) : m_pObject(pFrom) {}

  operator const void *() const { return m_pObject; }
  operator void *() { return m_pObject; }

  operator const T *() const { return m_pObject; }
  operator const T *() { return m_pObject; }
  operator T *() { return m_pObject; }

  int operator=(int i) {
    AssertMsg(i == 0, "Only nullptr allowed on integer assign.");
    m_pObject = 0;
    return 0;
  }
  T *operator=(T *p) {
    m_pObject = p;
    return p;
  }

  bool operator!() const { return !m_pObject; }
  bool operator!=(int i) const {
    AssertMsg(i == 0, "Only nullptr allowed on integer compare.");
    return (m_pObject != nullptr);
  }
  bool operator==(const void *p) const { return m_pObject == p; }
  bool operator!=(const void *p) const { return m_pObject != p; }
  bool operator==(T *p) const { return operator==((void *)p); }
  bool operator!=(T *p) const { return operator!=((void *)p); }
  bool operator==(const CBaseAutoPtr<T> &p) const {
    return operator==((const void *)p);
  }
  bool operator!=(const CBaseAutoPtr<T> &p) const {
    return operator!=((const void *)p);
  }

  T *operator->() { return m_pObject; }
  T &operator*() { return *m_pObject; }
  T **operator&() { return &m_pObject; }

  const T *operator->() const { return m_pObject; }
  const T &operator*() const { return *m_pObject; }
  T *const *operator&() const { return &m_pObject; }

 protected:
  CBaseAutoPtr(const CBaseAutoPtr<T> &from) : m_pObject(from.m_pObject) {}
  void operator=(const CBaseAutoPtr<T> &from) { m_pObject = from.m_pObject; }

  T *m_pObject;
};

template <typename T>
class CRefPtr : public CBaseAutoPtr<T> {
  using BaseClass = CBaseAutoPtr<T>;

 public:
  CRefPtr() {}
  CRefPtr(T *pInit) : BaseClass(pInit) {}
  CRefPtr(const CRefPtr<T> &from) : BaseClass(from) {}
  ~CRefPtr() {
    if (BaseClass::m_pObject) BaseClass::m_pObject->Release();
  }

  void operator=(const CRefPtr<T> &from) { BaseClass::operator=(from); }

  int operator=(int i) { return BaseClass::operator=(i); }
  T *operator=(T *p) { return BaseClass::operator=(p); }

  operator bool() const { return !BaseClass::operator!(); }
  operator bool() { return !BaseClass::operator!(); }

  void SafeRelease() {
    if (BaseClass::m_pObject) BaseClass::m_pObject->Release();
    BaseClass::m_pObject = 0;
  }
  void AssignAddRef(T *pFrom) {
    SafeRelease();
    if (pFrom) pFrom->AddRef();
    BaseClass::m_pObject = pFrom;
  }
  void AddRefAssignTo(T *&pTo) {
    ::SafeRelease(pTo);
    if (BaseClass::m_pObject) BaseClass::m_pObject->AddRef();
    pTo = BaseClass::m_pObject;
  }
};

// Traits classes defining reference count threading model
struct CRefMT {
  static int Increment(int *p) { return ThreadInterlockedIncrement((long *)p); }
  static int Decrement(int *p) { return ThreadInterlockedDecrement((long *)p); }
};

struct CRefST {
  static int Increment(int *p) { return ++(*p); }
  static int Decrement(int *p) { return --(*p); }
};

// Actual reference counting implementation. Pulled out to reduce
// code bloat.
template <const bool bSelfDelete, typename CRefThreading = CRefMT>
class MSVC_NOVTABLE CRefCountServiceBase {
 protected:
  CRefCountServiceBase() : refs_count_(1) {}

  virtual ~CRefCountServiceBase() {}

  virtual bool OnFinalRelease() { return true; }

  int GetRefCount() const { return refs_count_; }

  int DoAddRef() { return CRefThreading::Increment(&refs_count_); }

  int DoRelease() {
    int result = CRefThreading::Decrement(&refs_count_);

    if (result) return result;
    if (OnFinalRelease() && bSelfDelete) delete this;

    return 0;
  }

 private:
  int refs_count_;
};

class CRefCountServiceNull {
 protected:
  static int DoAddRef() { return 1; }
  static int DoRelease() { return 1; }
};

template <typename CRefThreading = CRefMT>
class MSVC_NOVTABLE CRefCountServiceDestruct {
 protected:
  CRefCountServiceDestruct() : refs_count_(1) {}

  virtual ~CRefCountServiceDestruct() {}

  int GetRefCount() const { return refs_count_; }

  int DoAddRef() { return CRefThreading::Increment(&refs_count_); }

  int DoRelease() {
    int result = CRefThreading::Decrement(&refs_count_);
    if (result) return result;
    this->~CRefCountServiceDestruct();
    return 0;
  }

 private:
  int refs_count_;
};

typedef CRefCountServiceBase<true, CRefST> CRefCountServiceST;
typedef CRefCountServiceBase<false, CRefST> CRefCountServiceNoDeleteST;

typedef CRefCountServiceBase<true, CRefMT> CRefCountServiceMT;
typedef CRefCountServiceBase<false, CRefMT> CRefCountServiceNoDeleteMT;

// Default to threadsafe
typedef CRefCountServiceNoDeleteMT CRefCountServiceNoDelete;
typedef CRefCountServiceMT CRefCountService;

// Base classes to implement reference counting
template <typename REFCOUNT_SERVICE = CRefCountService>
class MSVC_NOVTABLE CRefCounted : public REFCOUNT_SERVICE {
 public:
  virtual ~CRefCounted() {}
  int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
  int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <typename BASE1, typename REFCOUNT_SERVICE = CRefCountService>
class MSVC_NOVTABLE CRefCounted1 : public BASE1, public REFCOUNT_SERVICE {
 public:
  virtual ~CRefCounted1() {}
  int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
  int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <typename BASE1, typename BASE2,
          typename REFCOUNT_SERVICE = CRefCountService>
class MSVC_NOVTABLE CRefCounted2 : public BASE1,
                                   public BASE2,
                                   public REFCOUNT_SERVICE {
 public:
  virtual ~CRefCounted2() {}
  int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
  int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <typename BASE1, typename BASE2, typename BASE3,
          typename REFCOUNT_SERVICE = CRefCountService>
class MSVC_NOVTABLE CRefCounted3 : public BASE1,
                                   public BASE2,
                                   public BASE3,
                                   public REFCOUNT_SERVICE {
  virtual ~CRefCounted3() {}
  int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
  int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <typename BASE1, typename BASE2, typename BASE3, typename BASE4,
          typename REFCOUNT_SERVICE = CRefCountService>
class MSVC_NOVTABLE CRefCounted4 : public BASE1,
                                   public BASE2,
                                   public BASE3,
                                   public BASE4,
                                   public REFCOUNT_SERVICE {
 public:
  virtual ~CRefCounted4() {}
  int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
  int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

template <typename BASE1, typename BASE2, typename BASE3, typename BASE4,
          typename BASE5, typename REFCOUNT_SERVICE = CRefCountService>
class MSVC_NOVTABLE CRefCounted5 : public BASE1,
                                   public BASE2,
                                   public BASE3,
                                   public BASE4,
                                   public BASE5,
                                   public REFCOUNT_SERVICE {
 public:
  virtual ~CRefCounted5() {}
  int AddRef() { return REFCOUNT_SERVICE::DoAddRef(); }
  int Release() { return REFCOUNT_SERVICE::DoRelease(); }
};

// Class to throw around a reference counted item to debug
// referencing problems.
template <typename TBaseRefCounted, int FINAL_REFS = 0,
          const char *pszName = nullptr>
class CRefDebug : public TBaseRefCounted {
 public:
#ifdef _DEBUG
  CRefDebug() {
    AssertMsg(GetRefCount() == 1, "Expected initial ref count of 1");
    DevMsg("%s:create %p\n", (pszName) ? pszName : "", this);
  }

  virtual ~CRefDebug() {
    AssertDevMsg(GetRefCount() == FINAL_REFS,
                 "Object still referenced on destroy?");
    DevMsg("%s:destroy %p\n", (pszName) ? pszName : "", this);
  }

  int AddRef() {
    DevMsg("%s:(%p)->AddRef() --> %d\n", (pszName) ? pszName : "", this,
           GetRefCount() + 1);
    return BASE_REFCOUNTED::AddRef();
  }

  int Release() {
    DevMsg("%s:(%p)->Release() --> %d\n", (pszName) ? pszName : "", this,
           GetRefCount() - 1);
    Assert(GetRefCount() > 0);
    return BASE_REFCOUNTED::Release();
  }
#endif
};

#endif  // SOURCE_TIER1_REFCOUNT_H_
