// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
//=============================================================================

#ifndef CBASE_H
#define CBASE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef _DEBUG
#define DEBUG 1
#endif

// Misc C-runtime library headers
#include <math.h>
#include <stdio.h>
#include "minmax.h"

// tier 0
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"
#include "tier0/include/basetypes.h"

// tier 1
#include "tier1/strtools.h"
#include "utlvector.h"
#include "mathlib/vmatrix.h"
#include "FileSystem.h"

#include "tier1/ConVar.h"
#include "icvar.h"

#endif // CBASE_H
