// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Header file for optional HTTP asset downloading
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

void CL_HTTPStop_f();
bool CL_DownloadUpdate();
void CL_QueueDownload(const char *filename);
bool CL_FileReceived(const char *filename, unsigned int requestID);
bool CL_FileDenied(const char *filename, unsigned int requestID);
int CL_GetDownloadQueueSize();
int CL_CanUseHTTPDownload();
void CL_MarkMapAsUsingHTTPDownload();

#endif  // DOWNLOAD_H
