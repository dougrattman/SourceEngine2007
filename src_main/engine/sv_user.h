// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SV_USER_H
#define SV_USER_H

// Send a command to the specified client (as though the client typed the
// command in their console).
class client_t;
void SV_FinishParseStringCommand(client_t *cl, const char *pCommand);

#endif  // SV_USER_H
