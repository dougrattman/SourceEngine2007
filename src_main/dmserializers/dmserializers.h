// Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// Main header file for the serializers DLL

#ifndef DMSERIALIZERS_H
#define DMSERIALIZERS_H

class IDataModel;

//-----------------------------------------------------------------------------
// Externally defined importers
//-----------------------------------------------------------------------------
void InstallActBusyImporter(IDataModel *pFactory);
void InstallVMTImporter(IDataModel *pFactory);
void InstallSFMV1Importer(IDataModel *pFactory);
void InstallSFMV2Importer(IDataModel *pFactory);
void InstallSFMV3Importer(IDataModel *pFactory);
void InstallSFMV4Importer(IDataModel *pFactory);
void InstallSFMV5Importer(IDataModel *pFactory);
void InstallSFMV6Importer(IDataModel *pFactory);
void InstallSFMV7Importer(IDataModel *pFactory);
void InstallSFMV8Importer(IDataModel *pFactory);
void InstallSFMV9Importer(IDataModel *pFactory);
void InstallVMFImporter(IDataModel *pFactory);

void InstallDMXUpdater(IDataModel *pFactory);
void InstallSFMSessionUpdater(IDataModel *pFactory);
void InstallPCFUpdater(IDataModel *pFactory);

#endif  // DMSERIALIZERS_H
