// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// The keyboard input class
//
//=============================================================================

#ifndef DMEKEYBOARDINPUT_H
#define DMEKEYBOARDINPUT_H

#include "movieobjects/dmeinput.h"

//-----------------------------------------------------------------------------
// A class representing a camera
//-----------------------------------------------------------------------------
class CDmeKeyboardInput : public CDmeInput {
  DEFINE_ELEMENT(CDmeKeyboardInput, CDmeInput);

 public:
  virtual bool IsDirty();  // ie needs to operate
  virtual void Operate();

  virtual void GetInputAttributes(CUtlVector<CDmAttribute *> &attrs);
  virtual void GetOutputAttributes(CUtlVector<CDmAttribute *> &attrs);

 protected:
  CDmaVar<bool> *m_keys;

  bool GetKeyStatus(u32 ki);
};

#endif  // DMEKEYBOARDINPUT_H
