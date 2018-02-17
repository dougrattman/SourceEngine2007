// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:		Arbitrary length bit string
//				** NOTE: This class does NOT override the bitwise operators
//						 as doing so would require overriding the operators
//						 to allocate memory for the returned bitstring.  This method
//						 would be prone to memory leaks as the calling party
//						 would have to remember to delete the memory.  Funtions
//						 are used instead to require the calling party to allocate
//						 and destroy their own memory
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $


#include "cbase.h"

#include <climits>

#include "bitstring.h"
#include "tier1/utlbuffer.h"
#include "tier0/include/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

