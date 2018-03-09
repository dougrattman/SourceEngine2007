// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(ITHREAD_H)
#define ITHREAD_H

#include "tier1/interface.h"

// Typedef to point at member functions of CCDAudio
typedef void (CCDAudio::*vfunc)(int param1, int param2);

//-----------------------------------------------------------------------------
// Purpose: CD Audio thread processing
//-----------------------------------------------------------------------------
the_interface IThread {
 public:
  virtual ~IThread(void) {}

  virtual bool Init(void) = 0;
  virtual bool Shutdown(void) = 0;

  // Add specified item to thread for processing
  virtual bool AddThreadItem(vfunc pfn, int param1, int param2) = 0;
};

extern IThread *thread;

#endif  // ITHREAD_H
