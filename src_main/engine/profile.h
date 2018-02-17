// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(PROFILE_H)
#define PROFILE_H

void Host_ReadConfiguration(const bool readDefault = false);
void Host_WriteConfiguration(const char *dirname, const char *filename);

#endif  // PROFILE_H
