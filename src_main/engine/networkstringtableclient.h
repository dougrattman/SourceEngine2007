// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef NETWORKSTRINGTABLECLIENT_H
#define NETWORKSTRINGTABLECLIENT_H

class CUtlBuffer;

void CL_PrintStringTables(void);
void CL_WriteStringTables(CUtlBuffer& buf);
bool CL_ReadStringTables(CUtlBuffer& buf);

#endif  // NETWORKSTRINGTABLECLIENT_H
