// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#include "dt_recv_decoder.h"

#include "tier0/memdbgon.h"

CRecvDecoder::CRecvDecoder() {
  m_pTable = 0;
  m_pClientSendTable = 0;
}

CClientSendProp::CClientSendProp() { m_pTableName = 0; }

CClientSendProp::~CClientSendProp() { delete[] m_pTableName; }

CClientSendTable::CClientSendTable() {}

CClientSendTable::~CClientSendTable() {
  delete m_SendTable.m_pNetTableName;

  for (int iProp = 0; iProp < m_SendTable.m_nProps; iProp++) {
    delete[] m_SendTable.m_pProps[iProp].m_pVarName;
    delete[] m_SendTable.m_pProps[iProp].m_pExcludeDTName;
  }

  delete[] m_SendTable.m_pProps;
}
