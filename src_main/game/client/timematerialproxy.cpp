// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "FunctionProxy.h"

 
#include "tier0/include/memdbgon.h"

class CTimeMaterialProxy : public CResultProxy
{
public:
	virtual void OnBind( void *pC_BaseEntity );
};					    

void CTimeMaterialProxy::OnBind( void *pC_BaseEntity )
{
	SetFloatResult( gpGlobals->curtime );
}

EXPOSE_INTERFACE( CTimeMaterialProxy, IMaterialProxy, "CurrentTime" IMATERIAL_PROXY_INTERFACE_VERSION );
