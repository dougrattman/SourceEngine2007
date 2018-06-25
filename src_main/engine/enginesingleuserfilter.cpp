// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "enginesingleuserfilter.h"

#include "quakedef.h"
#include "server.h"

#include "tier0/include/memdbgon.h"

void SV_DetermineMulticastRecipients(
    bool usepas, const Vector& origin,
    CBitVec<ABSOLUTE_PLAYER_LIMIT>& playerbits);

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEngineRecipientFilter::CEngineRecipientFilter() { Reset(); }

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEngineRecipientFilter::Reset() {
  m_bReliable = false;
  m_bInit = false;
  m_Recipients.RemoveAll();
}

void CEngineRecipientFilter::MakeReliable() { m_bReliable = true; }

void CEngineRecipientFilter::MakeInitMessage() { m_bInit = true; }

int CEngineRecipientFilter::GetRecipientCount() const {
  return m_Recipients.Size();
}

int CEngineRecipientFilter::GetRecipientIndex(int slot) const {
  if (slot < 0 || slot >= GetRecipientCount()) return -1;

  return m_Recipients[slot];
}

void CEngineRecipientFilter::AddAllPlayers() {
  m_Recipients.RemoveAll();

  for (int i = 0; i < sv.GetClientCount(); i++) {
    IClient* cl = sv.GetClient(i);

    if (!cl->IsActive()) continue;

    m_Recipients.AddToTail(i + 1);
  }
}

void CEngineRecipientFilter::AddRecipient(int index) {
  // Already in list
  if (m_Recipients.Find(index) != m_Recipients.InvalidIndex()) return;

  m_Recipients.AddToTail(index);
}

void CEngineRecipientFilter::RemoveRecipient(int index) {
  // Remove it if it's in the list
  m_Recipients.FindAndRemove(index);
}

void CEngineRecipientFilter::AddPlayersFromBitMask(
    CBitVec<ABSOLUTE_PLAYER_LIMIT>& playerbits) {
  for (int i = 0; i < sv.GetClientCount(); i++) {
    if (!playerbits[i]) continue;

    IClient* cl = sv.GetClient(i);
    if (!cl->IsActive()) continue;

    AddRecipient(i + 1);
  }
}

bool CEngineRecipientFilter::IncludesPlayer(int playerindex) {
  for (int i = 0; i < GetRecipientCount(); i++) {
    if (playerindex == GetRecipientIndex(i)) return true;
  }

  return false;
}

void CEngineRecipientFilter::AddPlayersFromFilter(
    const IRecipientFilter* filter) {
  for (int i = 0; i < filter->GetRecipientCount(); i++) {
    AddRecipient(filter->GetRecipientIndex(i));
  }
}

void CEngineRecipientFilter::AddRecipientsByPVS(const Vector& origin) {
  if (sv.GetMaxClients() == 1) {
    AddAllPlayers();
  } else {
    CBitVec<ABSOLUTE_PLAYER_LIMIT> playerbits;
    SV_DetermineMulticastRecipients(false, origin, playerbits);
    AddPlayersFromBitMask(playerbits);
  }
}

void CEngineRecipientFilter::AddRecipientsByPAS(const Vector& origin) {
  if (sv.GetMaxClients() == 1) {
    AddAllPlayers();
  } else {
    CBitVec<ABSOLUTE_PLAYER_LIMIT> playerbits;
    SV_DetermineMulticastRecipients(true, origin, playerbits);
    AddPlayersFromBitMask(playerbits);
  }
}
