// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef COMMON_H
#define COMMON_H

#include "filesystem.h"
#include "gameui/IGameUI.h"
#include "mathlib/vector.h"  // NOTE(toml 05-01-02): solely for definition of QAngle
#include "qlimits.h"
#include "tier0/include/basetypes.h"
#include "worldsize.h"

class Vector;
struct cache_user_t;

#define COM_COPY_CHUNK_SIZE 1024  // For copying operations

#include "tier1/strtools.h"

// Allocates memory and copys text.
ch *COM_StringCopy(const ch *text);
// Frees memory allocated by COM_StringCopy.
void COM_StringFree(const ch *text);

extern void COM_WriteFile(const ch *filename, void *data, int len);
extern int COM_OpenFile(const ch *filename, FileHandle_t *file);
extern void COM_CloseFile(FileHandle_t hFile);
extern void COM_CreatePath(const ch *path);
extern int COM_FileSize(const ch *filename);
extern int COM_ExpandFilename(ch *filename, int maxlength);
extern uint8_t *COM_LoadFile(const ch *path, int usehunk, int *pLength);
extern bool COM_IsValidPath(const ch *pszFilename);

const ch *COM_Parse(const ch *data);
ch *COM_ParseLine(ch *data);
int COM_TokenWaiting(const ch *buffer);

extern bool com_ignorecolons;
extern ch com_token[1024];

void COM_Init();
void COM_Shutdown();
bool COM_CheckGameDirectory(const ch *gamedir);
void COM_ParseDirectoryFromCmd(const ch *pCmdName, ch *pDirName, int maxlen,
                               const ch *pDefault);

#define Bits2Bytes(b) ((b + 7) >> 3)

// does a varargs printf into a temp buffer
ch *va(const ch *format, ...);
// prints a vector into a temp buffer.
ch *vstr(Vector &v);

extern ch com_basedir[MAX_OSPATH];
extern ch com_gamedir[MAX_OSPATH];

uint8_t *COM_LoadStackFile(const ch *path, void *buffer, int bufsize,
                           int &filesize);
void COM_LoadCacheFile(const ch *path, cache_user_t *cu);
uint8_t *COM_LoadFile(const ch *path, int usehunk, int *pLength);

void COM_CopyFileChunk(FileHandle_t dst, FileHandle_t src, int nSize);
bool COM_CopyFile(const ch *netpath, const ch *cachepath);

void COM_SetupLogDir(const ch *mapname);
void COM_GetGameDir(ch *szGameDir, usize maxlen);
int COM_CompareFileTime(const ch *filename1, const ch *filename2,
                        int *iCompare);
int COM_GetFileTime(const ch *pFileName);
const ch *COM_ParseFile(const ch *data, ch *token, usize maxtoken);

extern ch gszDisconnectReason[256];
extern ch gszExtendedDisconnectReason[256];
extern bool gfExtendedError;
extern ESteamLoginFailure g_eSteamLoginFailure;
void COM_ExplainDisconnection(bool bPrint, const ch *fmt, ...);

// convert DX level to string.
const ch *COM_DXLevelToString(int dxlevel);

// Log a debug message to specified file (pszFile == NULL => c:\\hllog.txt).
void COM_Log(ch *pszFile, ch *fmt, ...);
void COM_LogString(ch const *pchFile, ch const *pchString);

// Returns seconds as hh:mm:ss string.
const ch *COM_FormatSeconds(int seconds);

// Return the mod dir (rather than the complete
// source::tier0::command_line_switches::kGamePath param, which can be a path).
const ch *COM_GetModDirectory();

#endif  // COMMON_H
