// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// An application system.

#ifndef SOURCE_APPFRAMEWORK_INCLUDE_IAPP_SYSTEM_H_
#define SOURCE_APPFRAMEWORK_INCLUDE_IAPP_SYSTEM_H_

#ifdef _WIN32
#pragma once
#endif

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

src_interface IAppSystem {
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
template <typename TAppSystem>
struct CBaseAppSystem : public TAppSystem {
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
template <typename TAppSystem>
class CTier0AppSystem : public CBaseAppSystem<TAppSystem> {
 public:
  CTier0AppSystem(bool is_primary_system = true)
      : is_primary_system_{is_primary_system} {}

 protected:
  // NOTE: a single DLL may have multiple AppSystems it's trying to expose. If
  // this is true, you must return true from only one of those AppSystems; not
  // doing so will cause all static libraries connected to it to
  // connect/disconnect multiple times,

  // NOTE: We don't do this as a virtual function to avoid having to up the
  // version on all interfaces.
  bool IsPrimaryAppSystem() const { return is_primary_system_; }

 private:
  const bool is_primary_system_;
};

#endif  // SOURCE_APPFRAMEWORK_INCLUDE_IAPP_SYSTEM_H_
