// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeoperator.h"
#include "datamodel/dmelementfactoryhelper.h"

 
#include "tier0/include/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeOperator, CDmeOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeOperator::OnConstruction()
{
}

void CDmeOperator::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeOperator::IsDirty()
{
	return BaseClass::IsDirty();
}
