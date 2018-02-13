// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $


#ifndef INTERVAL_H
#define INTERVAL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/include/basetypes.h"


interval_t ReadInterval( const char *pString );
float RandomInterval( const interval_t &interval );

#endif // INTERVAL_H
