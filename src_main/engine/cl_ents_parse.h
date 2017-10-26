// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef CL_ENTS_PARSE_H
#define CL_ENTS_PARSE_H

class CEntityReadInfo;

void CL_DeleteDLLEntity(int iEnt, const char *reason,
                        bool bOnRecreatingAllEntities = false);
void CL_CopyExistingEntity(CEntityReadInfo &u);
void CL_CopyNewEntity(CEntityReadInfo &u, int iClass, int iSerialNum);
void CL_PreprocessEntities(void);
bool CL_ProcessPacketEntities(SVC_PacketEntities *entmsg);

#endif  // CL_ENTS_PARSE_H
