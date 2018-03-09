// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"

 
#include "tier0/include/memdbgon.h"

class C_FuncMonitor : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncMonitor, C_BaseEntity );
	DECLARE_CLIENTCLASS();

// C_BaseEntity.
public:
	virtual bool	ShouldDraw();
};

IMPLEMENT_CLIENTCLASS_DT( C_FuncMonitor, DT_FuncMonitor, CFuncMonitor )
END_RECV_TABLE()

bool C_FuncMonitor::ShouldDraw()
{
	return true;
}
