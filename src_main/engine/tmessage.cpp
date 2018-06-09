// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tmessage.h"

#include "base/include/base_types.h"
#include "base/include/chrono.h"
#include "cdll_int.h"
#include "common.h"
#include "draw.h"
#include "mem_fgets.h"
#include "quakedef.h"
#include "tier0/include/icommandline.h"
#include "tier1/characterset.h"

#include "tier0/include/memdbgon.h"

#define MSGFILE_NAME 0
#define MSGFILE_TEXT 1
// I don't know if this table will balloon like every other feature in
// Half-Life But, for now, I've set this to a reasonable value
#define MAX_MESSAGES 600

// Defined in other files.
static characterset_t g_WhiteSpace;

client_textmessage_t gMessageParms;
client_textmessage_t *gMessageTable = nullptr;
int gMessageTableCount = 0;

ch gNetworkTextMessageBuffer[MAX_NETMESSAGE][512];
const ch *gNetworkMessageNames[MAX_NETMESSAGE] = {
    NETWORK_MESSAGE1, NETWORK_MESSAGE2, NETWORK_MESSAGE3,
    NETWORK_MESSAGE4, NETWORK_MESSAGE5, NETWORK_MESSAGE6};

client_textmessage_t gNetworkTextMessage[MAX_NETMESSAGE] = {
    0,  // effect
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    -1.0f,                        // x
    -1.0f,                        // y
    0.0f,                         // fadein
    0.0f,                         // fadeout
    0.0f,                         // holdtime
    0.0f,                         // fxTime,
    nullptr,                      // pVGuiSchemeFontName (nullptr == default)
    NETWORK_MESSAGE1,             // pName message name.
    gNetworkTextMessageBuffer[0]  // pMessage
};

ch gDemoMessageBuffer[512];
client_textmessage_t tm_demomessage = {
    0,  // effect
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    -1.0f,              // x
    -1.0f,              // y
    0.0f,               // fadein
    0.0f,               // fadeout
    0.0f,               // holdtime
    0.0f,               // fxTime,
    nullptr,            // pVGuiSchemeFontName (nullptr == default)
    DEMO_MESSAGE,       // pName message name.
    gDemoMessageBuffer  // pMessage
};

static client_textmessage_t orig_demo_message = tm_demomessage;

// The string "pText" is assumed to have all whitespace from both ends cut out
bool IsComment(ch *text) {
  if (text) {
    const usize length{strlen(text)};

    if (length >= 2 && text[0] == '/' && text[1] == '/') return true;

    // No text?
    if (length > 0) return false;
  }

  // No text is a comment too
  return true;
}

// The string "pText" is assumed to have all whitespace from both ends cut out
constexpr inline bool IsStartOfText(const ch *const text) {
  return text && text[0] == '{';
}

// The string "pText" is assumed to have all whitespace from both ends cut out
constexpr inline bool IsEndOfText(const ch *const text) {
  return text && text[0] == '}';
}

#define IsWhiteSpace(space) IN_CHARACTERSET(g_WhiteSpace, space)

const ch *SkipSpace(const ch *text) {
  if (text) {
    usize pos{0};

    while (text[pos] && IsWhiteSpace(text[pos])) pos++;

    return text + pos;
  }

  return nullptr;
}

const ch *SkipText(const ch *text) {
  if (text) {
    usize pos{0};

    while (text[pos] && !IsWhiteSpace(text[pos])) pos++;

    return text + pos;
  }

  return nullptr;
}

bool ParseFloats(const ch *text, f32 *float_val, usize count) {
  const ch *temp{text};
  u32 index{0};

  while (temp && count > 0) {
    // Skip current token / float
    // Skip any whitespace in between
    temp = SkipSpace(SkipText(temp));

    if (temp) {
      // Parse a float
      float_val[index] = strtof(temp, nullptr);

      --count;
      ++index;
    }
  }

  return count == 0;
}

template <usize out_size>
bool ParseString(const ch *text, ch (&out)[out_size]) {
  // Skip current token / float
  // Skip any whitespace in between
  const ch *temp{SkipSpace(SkipText(text))};

  if (temp) {
    ch const *start{temp};
    temp = SkipText(temp);

    const isize len{std::min(temp - start + 1, (isize)std::size(out) - 1)};
    strncpy_s(out, start, len);

    return true;
  }

  return false;
}

// Trims all whitespace from the front and end of a string
void TrimSpace(const ch *source, ch *dest) {
  isize start = 0;
  isize end = strlen(source);

  while (source[start] && IsWhiteSpace(source[start])) start++;

  end--;
  while (end > 0 && IsWhiteSpace(source[end])) end--;

  end++;

  isize length = end - start;

  if (length > 0)
    memcpy(dest, source + start, length);
  else
    length = 0;

  // Terminate the dest string
  dest[length] = '\0';
}

template <usize token_name_size>
inline bool IsToken(const ch *text, const ch (&token_name)[token_name_size]) {
  return text && token_name &&
         !_strnicmp(text + 1, token_name, token_name_size - 1);
}

static ch g_pchSkipName[64];

bool ParseDirective(const ch *text) {
  if (text && text[0] == '$') {
    f32 temp_f32[8];

    if (IsToken(text, "position")) {
      if (ParseFloats(text, temp_f32, 2)) {
        gMessageParms.x = temp_f32[0];
        gMessageParms.y = temp_f32[1];
      }
    } else if (IsToken(text, "effect")) {
      if (ParseFloats(text, temp_f32, 1)) {
        gMessageParms.effect = (int)temp_f32[0];
      }
    } else if (IsToken(text, "fxtime")) {
      if (ParseFloats(text, temp_f32, 1)) {
        gMessageParms.fxtime = temp_f32[0];
      }
    } else if (IsToken(text, "color2")) {
      if (ParseFloats(text, temp_f32, 3)) {
        gMessageParms.r2 = (int)temp_f32[0];
        gMessageParms.g2 = (int)temp_f32[1];
        gMessageParms.b2 = (int)temp_f32[2];
      }
    } else if (IsToken(text, "color")) {
      if (ParseFloats(text, temp_f32, 3)) {
        gMessageParms.r1 = (int)temp_f32[0];
        gMessageParms.g1 = (int)temp_f32[1];
        gMessageParms.b1 = (int)temp_f32[2];
      }
    } else if (IsToken(text, "fadein")) {
      if (ParseFloats(text, temp_f32, 1)) {
        gMessageParms.fadein = temp_f32[0];
      }
    } else if (IsToken(text, "fadeout")) {
      if (ParseFloats(text, temp_f32, 3)) {
        gMessageParms.fadeout = temp_f32[0];
      }
    } else if (IsToken(text, "holdtime")) {
      if (ParseFloats(text, temp_f32, 3)) {
        gMessageParms.holdtime = temp_f32[0];
      }
    } else if (IsToken(text, "boxsize")) {
      if (ParseFloats(text, temp_f32, 1)) {
        gMessageParms.bRoundedRectBackdropBox = temp_f32[0] != 0.0f;
        gMessageParms.flBoxSize = temp_f32[0];
      }
    } else if (IsToken(text, "boxcolor")) {
      if (ParseFloats(text, temp_f32, 4)) {
        gMessageParms.boxcolor[0] = (u8)(int)temp_f32[0];
        gMessageParms.boxcolor[1] = (u8)(int)temp_f32[1];
        gMessageParms.boxcolor[2] = (u8)(int)temp_f32[2];
        gMessageParms.boxcolor[3] = (u8)(int)temp_f32[3];
      }
    } else if (IsToken(text, "clearmessage")) {
      if (ParseString(text, g_pchSkipName)) {
        gMessageParms.pClearMessage =
            !g_pchSkipName[0] || !Q_stricmp(g_pchSkipName, "0") ? nullptr
                                                                : g_pchSkipName;
      }
    } else {
      ConDMsg("tmessage(scripts/titles.txt): Unknown token: %s\n", text);
    }

    return true;
  }

  return false;
}

void TextMessageParseLog(const client_textmessage_t *m,
                         const usize messages_count) {
  Msg("%zu %s\n", messages_count, m->pName ? m->pName : "(0)");
  Msg("  effect %d, color1(%d,%d,%d,%d), color2(%d,%d,%d,%d)\n", m->effect,
      m->r1, m->g1, m->b1, m->a1, m->r2, m->g2, m->b2, m->a2);
  Msg("  pos %f,%f, fadein %f fadeout %f hold %f fxtime %f\n", m->x, m->y,
      m->fadein, m->fadeout, m->holdtime, m->fxtime);
  Msg("  '%s'\n", m->pMessage ? m->pMessage : "(0)");

  Msg("  box %s, size %f, color(%d,%d,%d,%d)\n",
      m->bRoundedRectBackdropBox ? "yes" : "no", m->flBoxSize, m->boxcolor[0],
      m->boxcolor[1], m->boxcolor[2], m->boxcolor[3]);

  if (m->pClearMessage) {
    Msg("  will clear '%s'\n", m->pClearMessage);
  }
}

void TextMessageParse(u8 *mem_file, usize mem_file_size) {
  ch *pCurrentText = 0, *the_name_heap;
  ch current_name[512], name_heap[16384];

  client_textmessage_t text_messages[MAX_MESSAGES];

  usize last_name_idx = 0, line_num = 0;
  usize file_pos = 0, last_line_pos = 0;
  usize message_idx = 0;

  const bool do_debug_log =
      CommandLine()->FindParm("-textmessagedebug") ? true : false;

  CharacterSetBuild(&g_WhiteSpace, " \r\n\t");

  int mode = MSGFILE_NAME;  // Searching for a message name

  ch file_buffer[512], trim[512];
  while (memfgets(mem_file, mem_file_size, &file_pos, file_buffer,
                  std::size(file_buffer)) != nullptr) {
    TrimSpace(file_buffer, trim);

    switch (mode) {
      case MSGFILE_NAME:
        if (IsComment(trim))  // Skip comment lines
          break;

        // Is this a directive "$command"?, if so parse it and break
        if (ParseDirective(trim)) break;

        if (IsStartOfText(trim)) {
          mode = MSGFILE_TEXT;
          pCurrentText = (ch *)(mem_file + file_pos);
          break;
        }

        if (IsEndOfText(trim)) {
          ConDMsg(
              "tmessage(scripts/titles.txt): Unexpected '}' found, line %zu\n",
              line_num);
          return;
        }

        strcpy_s(current_name, trim);
        break;

      case MSGFILE_TEXT:
        if (IsEndOfText(trim)) {
          usize length = strlen(current_name);
          usize now_name_heap_size{std::size(name_heap) -
                                   (&name_heap[last_name_idx] - name_heap)};

          // Save name on name heap
          if (last_name_idx + length > 8192) {
            ConDMsg(
                "tmessage(scripts/titles.txt): Too many titles, skipping...\n");
            return;
          }

          strcpy_s(name_heap + last_name_idx, now_name_heap_size, current_name);

          // Terminate text in-place in the memory file (it's temporary memory
          // that will be deleted) If the string starts with #, it's a
          // localization string and we don't want the \n on the end or the
          // Find() lookup will fail (so subtract 2)
          if (pCurrentText && pCurrentText[0] && pCurrentText[0] == '#' &&
              last_line_pos > 1) {
            mem_file[last_line_pos - 2] = 0;
          } else {
            mem_file[last_line_pos - 1] = 0;
          }

          // Save name/text on heap
          text_messages[message_idx] = gMessageParms;
          text_messages[message_idx].pName = name_heap + last_name_idx;

          last_name_idx += strlen(current_name) + 1;

          if (gMessageParms.pClearMessage) {
            Q_strncpy(name_heap + last_name_idx,
                      text_messages[message_idx].pClearMessage,
                      strlen(text_messages[message_idx].pClearMessage) + 1);
            text_messages[message_idx].pClearMessage =
                name_heap + last_name_idx;
            last_name_idx +=
                strlen(text_messages[message_idx].pClearMessage) + 1;
          }

          text_messages[message_idx].pMessage = pCurrentText;

          if (!do_debug_log) {
            // Branch prediction suggestion.
          } else {
            TextMessageParseLog(&text_messages[message_idx], message_idx);
          }

          ++message_idx;

          // Reset parser to search for names
          mode = MSGFILE_NAME;
          break;
        }

        if (IsStartOfText(trim)) {
          ConDMsg(
              "tmessage(scripts/titles.txt): Unexpected '{' found, line %zu\n",
              line_num);
          return;
        }

        break;
    }

    line_num++;
    last_line_pos = file_pos;

    if (message_idx >= MAX_MESSAGES) {
      Sys_Error(
          "tmessage(scripts/titles.txt): Too many messages (%zu > %zu).\n",
          message_idx + 1, MAX_MESSAGES);
      break;
    }
  }

  ConDMsg("tmessage(scripts/titles.txt): Parsed %zu text messages.\n",
          message_idx);

  usize nameHeapSize = last_name_idx;
  usize textHeapSize = 0;

  for (usize i = 0; i < message_idx; i++) {
    textHeapSize += strlen(text_messages[i].pMessage) + 1;
  }

  usize messageSize = message_idx * sizeof(client_textmessage_t);

  // Must malloc because we need to be able to clear it after initialization
  usize size = textHeapSize + nameHeapSize + messageSize;
  gMessageTable = heap_alloc<client_textmessage_t>(size);

  // Copy table over
  memcpy(gMessageTable, text_messages, messageSize);

  // Copy Name heap
  the_name_heap = ((ch *)gMessageTable) + messageSize;
  memcpy(the_name_heap, name_heap, nameHeapSize);
  usize nameOffset = the_name_heap - gMessageTable[0].pName;

  // Copy text & fixup pointers
  pCurrentText = the_name_heap + nameHeapSize;

  for (usize i = 0; i < message_idx; i++) {
    // Adjust name pointer (parallel buffer)
    gMessageTable[i].pName += nameOffset;

    if (gMessageTable[i].pClearMessage) {
      gMessageTable[i].pClearMessage += nameOffset;
    }

    // Copy text over
    strcpy_s(pCurrentText,
             (uintptr_t)gMessageTable + size - (uintptr_t)pCurrentText,
             gMessageTable[i].pMessage);

    gMessageTable[i].pMessage = pCurrentText;
    pCurrentText += strlen(pCurrentText) + 1;
  }

#ifndef NDEBUG
  if ((pCurrentText - (ch *)gMessageTable) !=
      (textHeapSize + nameHeapSize + messageSize))
    ConMsg("Overflow text message buffer!!!!!\n");
#endif

  gMessageTableCount = message_idx;
}

void TextMessageShutdown() {
  // Clear out any old data that's sitting around.
  heap_free(gMessageTable);
}

void TextMessageInit() {
  // Clear out any old data that's sitting around.
  heap_free(gMessageTable);

  int fileSize;
  u8 *mem_file = COM_LoadFile("scripts/titles.txt", 5, &fileSize);

  if (mem_file) {
    f64 parse_stamp;
    source::chrono::HpetTimer::TimeIt(
        parse_stamp, [=]() { TextMessageParse(mem_file, fileSize); });

    ConDMsg("tmessage(scripts/titles.txt): Parsing took %.3f sec.\n",
            parse_stamp);

    heap_free(mem_file);
  }

  usize i{0};
  for (auto &message : gNetworkTextMessage) {
    message.pMessage = gNetworkTextMessageBuffer[i++];
  }
}

void TextMessage_DemoMessage(const ch *pszMessage, float fFadeInTime,
                             float fFadeOutTime, float fHoldTime) {
  if (!pszMessage || !pszMessage[0]) return;

  strcpy_s(gDemoMessageBuffer, pszMessage);

  // Restore
  tm_demomessage = orig_demo_message;
  tm_demomessage.fadein = fFadeInTime;
  tm_demomessage.fadeout = fFadeOutTime;
  tm_demomessage.holdtime = fHoldTime;
}

void TextMessage_DemoMessageFull(const ch *pszMessage,
                                 client_textmessage_t const *message) {
  Assert(message);
  if (!message || !pszMessage || !pszMessage[0]) return;

  memcpy(&tm_demomessage, message, sizeof(tm_demomessage));
  tm_demomessage.pMessage = orig_demo_message.pMessage;
  tm_demomessage.pName = orig_demo_message.pName;

  strcpy_s(gDemoMessageBuffer, pszMessage);
}

client_textmessage_t *TextMessageGet(const ch *pName) {
  if (!_stricmp(pName, DEMO_MESSAGE)) return &tm_demomessage;

  // HACKHACK -- add 4 "channels" of network text
  if (!_stricmp(pName, NETWORK_MESSAGE1))
    return gNetworkTextMessage;
  else if (!_stricmp(pName, NETWORK_MESSAGE2))
    return gNetworkTextMessage + 1;
  else if (!_stricmp(pName, NETWORK_MESSAGE3))
    return gNetworkTextMessage + 2;
  else if (!_stricmp(pName, NETWORK_MESSAGE4))
    return gNetworkTextMessage + 3;
  else if (!_stricmp(pName, NETWORK_MESSAGE5))
    return gNetworkTextMessage + 4;
  else if (!_stricmp(pName, NETWORK_MESSAGE6))
    return gNetworkTextMessage + 5;

  for (int i = 0; i < gMessageTableCount; i++) {
    if (!_stricmp(pName, gMessageTable[i].pName)) return &gMessageTable[i];
  }

  return nullptr;
}
