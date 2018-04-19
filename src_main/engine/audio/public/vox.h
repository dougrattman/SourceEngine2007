// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VOX_H
#define VOX_H

#ifdef __cplusplus
extern "C" {
#endif

struct sfxcache_t;
struct channel_t;

class CUtlSymbol;

extern void VOX_Init(void);
extern void VOX_Shutdown(void);
extern void VOX_ReadSentenceFile(const ch *psentenceFileName);
extern int VOX_SentenceCount(void);
extern void VOX_LoadSound(channel_t *pchan, const ch *psz);
// UNDONE: Improve the interface of this call, it returns sentence data AND the
// sentence index
extern ch *VOX_LookupString(const ch *pSentenceName, int *psentencenum,
                              bool *pbEmitCaption = NULL,
                              CUtlSymbol *pCaptionSymbol = NULL,
                              float *pflDuration = NULL);
extern void VOX_PrecacheSentenceGroup(class IEngineSound *pSoundSystem,
                                      const ch *pGroupName,
                                      const ch *pPathOverride = NULL);
extern const ch *VOX_SentenceNameFromIndex(int sentencenum);
extern float VOX_SentenceLength(int sentence_num);
extern const ch *VOX_GroupNameFromIndex(int groupIndex);
extern int VOX_GroupIndexFromName(const ch *pGroupName);
extern int VOX_GroupPick(int isentenceg, ch *szfound, int strLen);
extern int VOX_GroupPickSequential(int isentenceg, ch *szfound,
                                   int szfoundLen, int ipick, int freset);

#ifdef __cplusplus
}
#endif

#endif  // VOX_H
