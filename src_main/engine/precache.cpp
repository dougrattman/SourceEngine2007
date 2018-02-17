// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "precache.h"

#include "quakedef.h"

#include "tier0/include/memdbgon.h"

// Purpose: Print out flag names
char const *GetFlagString(int flags) {
  static char ret[512];
  ret[0] = 0;

  bool first = true;

  if (!flags) return "None";

  if (flags & RES_FATALIFMISSING) {
    if (!first) {
      Q_strncat(ret, " | ", sizeof(ret), COPY_ALL_CHARACTERS);
    }
    Q_strncat(ret, "RES_FATALIFMISSING", sizeof(ret), COPY_ALL_CHARACTERS);
    first = false;
  }

  if (flags & RES_PRELOAD) {
    if (!first) {
      Q_strncat(ret, " | ", sizeof(ret), COPY_ALL_CHARACTERS);
    }
    Q_strncat(ret, "RES_PRELOAD", sizeof(ret), COPY_ALL_CHARACTERS);
    first = false;
  }

  return ret;
}

CPrecacheItem::CPrecacheItem(void) {
  Init(TYPE_UNK, NULL);
  ResetStats();
}

void CPrecacheItem::ResetStats(void) {
  m_uiRefcount = 0;
#if DEBUG_PRECACHE
  m_flFirst = 0.0f;
  m_flMostRecent = 0.0f;
#endif
}

void CPrecacheItem::Reference(void) {
  m_uiRefcount++;
#if DEBUG_PRECACHE
  m_flMostRecent = realtime;
  if (!m_flFirst) {
    m_flFirst = realtime;
  }
#endif
}

void CPrecacheItem::Init(int type, void const *ptr) {
  m_nType = type;
  u.model = (model_t *)ptr;
  if (ptr) {
    ResetStats();
  }
}

model_t *CPrecacheItem::GetModel(void) {
  if (!u.model) return NULL;

  Assert(m_nType == TYPE_MODEL);

  Reference();

  return u.model;
}

char const *CPrecacheItem::GetGeneric(void) {
  if (!u.generic) return NULL;

  Assert(m_nType == TYPE_GENERIC);

  Reference();

  return u.generic;
}

CSfxTable *CPrecacheItem::GetSound(void) {
  if (!u.sound) return NULL;

  Assert(m_nType == TYPE_SOUND);

  Reference();

  return u.sound;
}

char const *CPrecacheItem::GetName(void) {
  if (!u.name) return NULL;

  Assert(m_nType == TYPE_SOUND);

  Reference();

  return u.name;
}

char const *CPrecacheItem::GetDecal(void) {
  if (!u.name) return NULL;

  Assert(m_nType == TYPE_DECAL);

  Reference();

  return u.name;
}

void CPrecacheItem::SetModel(model_t const *pmodel) {
  Init(TYPE_MODEL, pmodel);
}

void CPrecacheItem::SetGeneric(char const *pname) { Init(TYPE_GENERIC, pname); }

void CPrecacheItem::SetSound(CSfxTable const *psound) {
  Init(TYPE_SOUND, psound);
}

void CPrecacheItem::SetName(char const *name) { Init(TYPE_SOUND, name); }

void CPrecacheItem::SetDecal(char const *decalname) {
  Init(TYPE_DECAL, decalname);
}

float CPrecacheItem::GetFirstReference(void) {
#if DEBUG_PRECACHE
  return m_flFirst;
#else
  return 0;
#endif
}

float CPrecacheItem::GetMostRecentReference(void) {
#if DEBUG_PRECACHE
  return m_flMostRecent;
#else
  return 0;
#endif
}

unsigned int CPrecacheItem::GetReferenceCount(void) { return m_uiRefcount; }
