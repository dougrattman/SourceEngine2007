// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SV_UPLOADGAMESTATS_H
#define SV_UPLOADGAMESTATS_H

bool UploadGameStats(char const *cserIP, unsigned int buildnumber,
                     char const *exe, char const *gamedir, char const *mapname,
                     unsigned int blobversion, unsigned int blobsize,
                     const void *pvBlobData)

#endif  // SV_UPLOADGAMESTATS_H
