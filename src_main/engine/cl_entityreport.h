// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_CL_ENTITYREPORT_H
#define SOURCE_CL_ENTITYREPORT_H

class ClientClass;

// Bits needed for action
void CL_RecordEntityBits(int entnum, int bitcount);

// Special events to record
void CL_RecordAddEntity(int entnum);
void CL_RecordLeavePVS(int entnum);
void CL_RecordDeleteEntity(int entnum, ClientClass *pclass);

extern ConVar cl_entityreport;

#endif  // SOURCE_CL_ENTITYREPORT_H