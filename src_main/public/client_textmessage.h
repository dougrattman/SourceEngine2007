// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose:
//

#ifndef CLIENT_TEXTMESSAGE_H
#define CLIENT_TEXTMESSAGE_H

#include <cstdint>

struct client_textmessage_t {
  int effect;
  uint8_t r1, g1, b1, a1;  // 2 colors for effects
  uint8_t r2, g2, b2, a2;
  float x;
  float y;
  float fadein;
  float fadeout;
  float holdtime;
  float fxtime;
  const char *pVGuiSchemeFontName;  // If 0, use default font for messages
  const char *pName;
  const char *pMessage;
  bool bRoundedRectBackdropBox;
  float flBoxSize;  // as a function of font height
  uint8_t boxcolor[4];
  char const *pClearMessage;  // message to clear
};

#endif  // CLIENT_TEXTMESSAGE_H
