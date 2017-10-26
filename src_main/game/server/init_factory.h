// Copyright � 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $


#ifndef INIT_FACTORY_H
#define INIT_FACTORY_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/interface.h"

struct factorylist_t
{
	CreateInterfaceFn engineFactory;
	CreateInterfaceFn physicsFactory;
	CreateInterfaceFn fileSystemFactory;
};

// Store off the factories
void FactoryList_Store( const factorylist_t &sourceData );

// retrieve the stored factories
void FactoryList_Retrieve( factorylist_t &destData );

#endif // INIT_FACTORY_H
