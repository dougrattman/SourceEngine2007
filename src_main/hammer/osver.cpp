// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
#include "stdafx.h"

#include "osver.h"

static eOSVersion s_OS = eUninitialized;

void initOSVersion() { s_OS = eWinNT; }

eOSVersion getOSVersion() {
  if (s_OS == eUninitialized) {
    initOSVersion();
  }
  return s_OS;
}
