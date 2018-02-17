// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALPROXYFACTORY_H
#define MATERIALPROXYFACTORY_H

#include "materialsystem/imaterialproxyfactory.h"
#include "tier1/interface.h"

class CMaterialProxyFactory : public IMaterialProxyFactory {
 public:
  IMaterialProxy *CreateProxy(const char *proxyName);
  void DeleteProxy(IMaterialProxy *pProxy);

 private:
  IMaterialProxy *LookupProxy(const char *proxyName, CreateInterfaceFn factory);
};

#endif  // MATERIALPROXYFACTORY_H
