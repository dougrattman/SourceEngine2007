// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: An application framework.

#ifndef SOURCE_APPFRAMEWORK_IAPPSYSTEM_H_
#define SOURCE_APPFRAMEWORK_IAPPSYSTEM_H_

#include "base/include/base_types.h"

#include "tier1/interface.h"

// Client systems are singleton objects in the client code base responsible for
// various tasks. The order in which the client systems appear in this list are
// the order in which they are initialized and updated. They are shut down in
// reverse order from which they are initialized.
enum InitReturnVal_t {
  INIT_FAILED = 0,
  INIT_OK,

  INIT_LAST_VAL,
};

the_interface IAppSystem {
 public:
  // Here's where the app systems get to learn about each other.
  virtual bool Connect(CreateInterfaceFn factory) = 0;
  virtual void Disconnect() = 0;

  // Here's where systems can access other interfaces implemented by this object
  // Returns nullptr if it doesn't implement the requested interface.
  virtual void *QueryInterface(const ch *interface_name) = 0;

  // Init, shutdown.
  virtual InitReturnVal_t Init() = 0;
  virtual void Shutdown() = 0;
};

// Helper empty implementation of an IAppSystem.
template <class IInterface>
class CBaseAppSystem : public IInterface {
 public:
  // Here's where the app systems get to learn about each other.
  virtual bool Connect(CreateInterfaceFn factory) { return true; }
  virtual void Disconnect() {}

  // Here's where systems can access other interfaces implemented by this object
  // Returns nullptr if it doesn't implement the requested interface.
  virtual void *QueryInterface(const ch *interface_name) { return nullptr; }

  // Init, shutdown.
  virtual InitReturnVal_t Init() { return INIT_OK; }
  virtual void Shutdown() {}
};

// Helper implementation of an IAppSystem for tier0.
template <class IInterface>
class CTier0AppSystem : public CBaseAppSystem<IInterface> {
 public:
  CTier0AppSystem(bool is_primary_app_system = true)
      : m_bIsPrimaryAppSystem{is_primary_app_system} {}

 protected:
  // NOTE: a single DLL may have multiple AppSystems it's trying to expose. If
  // this is true, you must return true from only one of those AppSystems; not
  // doing so will cause all static libraries connected to it to
  // connect/disconnect multiple times,

  // NOTE: We don't do this as a virtual function to avoid having to up the
  // version on all interfaces.
  bool IsPrimaryAppSystem() const { return m_bIsPrimaryAppSystem; }

 private:
  const bool m_bIsPrimaryAppSystem;
};

// This is the version of IAppSystem shipped 10/15/04
// NOTE: Never change this!!!
the_interface IAppSystemV0 {
 public:
  // Here's where the app systems get to learn about each other.
  virtual bool Connect(CreateInterfaceFn factory) = 0;
  virtual void Disconnect() = 0;

  // Here's where systems can access other interfaces implemented by this object
  // Returns nullptr if it doesn't implement the requested interface.
  virtual void *QueryInterface(const ch *interface_name) = 0;

  // Init, shutdown.
  virtual InitReturnVal_t Init() = 0;
  virtual void Shutdown() = 0;
};

#endif  // SOURCE_APPFRAMEWORK_IAPPSYSTEM_H_
