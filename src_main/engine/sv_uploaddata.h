// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SV_UPLOADDATA_H
#define SV_UPLOADDATA_H

class KeyValues;

bool UploadData(char const *cserIP, char const *tablename, KeyValues *fields);

#endif  // SV_UPLOADDATA_H
