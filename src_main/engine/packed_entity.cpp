// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "packed_entity.h"

#include <malloc.h>
#include <cassert>
#include <cstring>
#include "changeframelist.h"
#include "dt_send.h"
#include "dt_send_eng.h"
#include "server_class.h"
#include "tier0/include/basetypes.h"

#include "tier0/include/memdbgon.h"

PackedEntity::PackedEntity() {
  m_pData = NULL;
  m_pChangeFrameList = NULL;
  m_nSnapshotCreationTick = 0;
  m_nShouldCheckCreationTick = 0;
}

PackedEntity::~PackedEntity() {
  FreeData();

  if (m_pChangeFrameList) {
    m_pChangeFrameList->Release();
    m_pChangeFrameList = NULL;
  }
}

bool PackedEntity::AllocAndCopyPadded(const void *pData, unsigned long size) {
  FreeData();

  unsigned long nBytes = PAD_NUMBER(size, 4);

  // allocate the memory
  m_pData = malloc(nBytes);

  if (!m_pData) {
    Assert(m_pData);
    return false;
  }

  Q_memcpy(m_pData, pData, size);
  SetNumBits(nBytes * 8);

  return true;
}

int PackedEntity::GetPropsChangedAfterTick(int iTick, int *iOutProps,
                                           int nMaxOutProps) {
  if (m_pChangeFrameList) {
    return m_pChangeFrameList->GetPropsChangedAfterTick(iTick, iOutProps,
                                                        nMaxOutProps);
  } else {
    // signal that we don't have a changelist
    return -1;
  }
}

const CSendProxyRecipients *PackedEntity::GetRecipients() const {
  return m_Recipients.Base();
}

int PackedEntity::GetNumRecipients() const { return m_Recipients.Count(); }

void PackedEntity::SetRecipients(
    const CUtlMemory<CSendProxyRecipients> &recipients) {
  m_Recipients.CopyArray(recipients.Base(), recipients.Count());
}

bool PackedEntity::CompareRecipients(
    const CUtlMemory<CSendProxyRecipients> &recipients) {
  if (recipients.Count() != m_Recipients.Count()) return false;

  return memcmp(recipients.Base(), m_Recipients.Base(),
                sizeof(CSendProxyRecipients) * m_Recipients.Count()) == 0;
}

void PackedEntity::SetServerAndClientClass(ServerClass *pServerClass,
                                           ClientClass *pClientClass) {
  m_pServerClass = pServerClass;
  m_pClientClass = pClientClass;
  if (pServerClass) {
    Assert(pServerClass->m_pTable);
    SetShouldCheckCreationTick(
        pServerClass->m_pTable->HasPropsEncodedAgainstTickCount());
  }
}
