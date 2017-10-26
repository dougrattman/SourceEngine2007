// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "gametrace.h"

#include "eiface.h"
#include "server.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CGameTrace::SetEdict(edict_t* pEdict) {
  m_pEnt = serverGameEnts->EdictToBaseEntity(pEdict);
}

edict_t* CGameTrace::GetEdict() const {
  return serverGameEnts->BaseEntityToEdict(m_pEnt);
}
