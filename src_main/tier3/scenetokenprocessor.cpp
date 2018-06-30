// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "../game/shared/iscenetokenprocessor.h"

#include "../game/shared/choreoactor.h"
#include "../game/shared/choreochannel.h"
#include "../game/shared/choreoevent.h"
#include "../game/shared/choreoscene.h"
#include "tier1/characterset.h"

// Purpose: Helper for parsing scene data file
class CSceneTokenProcessor : public ISceneTokenProcessor {
 public:
  CSceneTokenProcessor();

  const char *CurrentToken();
  bool GetToken(bool crossline);
  bool TokenAvailable();
  void Error(const char *fmt, ...);
  void SetBuffer(char *buffer);

 private:
  const char *ParseNextToken(const char *data);

  const char *buffer_;
  char the_token_[1024];

  characterset_t m_BreakSetIncludingColons;
};

CSceneTokenProcessor::CSceneTokenProcessor() : buffer_{nullptr} {
  CharacterSetBuild(&m_BreakSetIncludingColons, "{}()':");
}

const char *CSceneTokenProcessor::CurrentToken() { return the_token_; }

const char *CSceneTokenProcessor::ParseNextToken(const char *data) {
  unsigned char c;
  characterset_t *breaks = &m_BreakSetIncludingColons;

  the_token_[0] = '\0';

  if (!data) return nullptr;

  usize len = 0;

// skip whitespace
skipwhite:
  while ((c = *data) <= ' ') {
    if (c == 0) return nullptr;  // end of file;
    data++;
  }

  // skip // comments
  if (c == '/' && data[1] == '/') {
    while (*data && *data != '\n') data++;
    goto skipwhite;
  }

  // handle quoted strings specially
  if (c == '\"') {
    data++;

    while (true) {
      c = *data++;

      if (c == '\"' || !c) {
        the_token_[len] = '\0';
        return data;
      }

      the_token_[len] = c;
      len++;
    }
  }

  // parse single characters
  if (IN_CHARACTERSET(*breaks, c)) {
    the_token_[len] = c;
    len++;
    the_token_[len] = '\0';
    return data + 1;
  }

  // parse a regular word
  do {
    the_token_[len] = c;
    data++;
    len++;
    c = *data;

    if (IN_CHARACTERSET(*breaks, c)) break;
  } while (c > 32);

  the_token_[len] = '\0';
  return data;
}

bool CSceneTokenProcessor::GetToken(bool crossline) {
  // NOTE: crossline is ignored here, may need to implement if needed
  buffer_ = ParseNextToken(buffer_);
  return the_token_[0] != '\0';
}

bool CSceneTokenProcessor::TokenAvailable() {
  const char *search_p = buffer_;

  while (*search_p <= 32) {
    if (*search_p == '\n') return false;
    search_p++;
    if (!*search_p) return false;
  }

  if (*search_p == ';' ||
      *search_p == '#' ||  // semicolon and # is comment field
      (*search_p == '/' &&
       *((search_p) + 1) == '/'))  // also make // a comment field
    return false;

  return true;
}

void CSceneTokenProcessor::Error(const char *fmt, ...) {
  char string[2048];

  va_list argptr;
  va_start(argptr, fmt);
  vsprintf_s(string, fmt, argptr);
  va_end(argptr);

  Warning("%s", string);
  Assert(0);
}

void CSceneTokenProcessor::SetBuffer(char *buffer) { buffer_ = buffer; }

CSceneTokenProcessor g_TokenProcessor;

ISceneTokenProcessor *GetTokenProcessor() { return &g_TokenProcessor; }

void SetTokenProcessorBuffer(const char *buf) {
  g_TokenProcessor.SetBuffer((char *)buf);
}
