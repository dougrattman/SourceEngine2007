// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// hltvtest.h: hltv test system

#ifndef HLTVTEST_H
#define HLTVTEST_H

#include "tier1/UtlVector.h"

class CHLTVServer;

class CHLTVTestSystem {
 public:
  CHLTVTestSystem();
  ~CHLTVTestSystem();

  void RunFrame();
  bool StartTest(int nClients, const char *pszAddress);
  void RetryTest(int nClients);
  bool StopsTest();

 protected:
  CUtlVector<CHLTVServer *> m_Servers;
};

extern CHLTVTestSystem
    *hltvtest;  // The global HLTV server/object. NULL on xbox.

#endif  // HLTVSERVER_H
