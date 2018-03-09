// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "c_physbox.h"

 
#include "tier0/include/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_PhysBox, DT_PhysBox, CPhysBox)
	RecvPropFloat(RECVINFO(m_mass), 0), // Test..
END_RECV_TABLE()


C_PhysBox::C_PhysBox()
{
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_PhysBox::ShadowCastType()
{
	if (IsEffectActive(EF_NODRAW | EF_NOSHADOW))
		return SHADOWS_NONE;
	return SHADOWS_RENDER_TO_TEXTURE;
}

C_PhysBox::~C_PhysBox()
{
}

