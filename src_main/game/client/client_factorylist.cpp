// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "client_factorylist.h"

 
#include "tier0/include/memdbgon.h"

static factorylist_t s_factories;

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData )
{
	s_factories = sourceData;
}

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData )
{
	destData = s_factories;
}
