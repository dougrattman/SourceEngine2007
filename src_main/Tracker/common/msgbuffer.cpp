// Copyright © 1996-2001, Valve LLC, All rights reserved.

#include "msgbuffer.h"

#include <cassert>
#include <cstring>

#include "tier0/include/memdbgon.h"

// Purpose: Allocate message buffer
CMsgBuffer::CMsgBuffer(const char *buffername,
                       void (*ef)(const char *fmt, ...) /*= NULL*/) {
  m_pszBufferName = buffername;
  m_pfnErrorFunc = ef;
  m_bAllowOverflow = false;  // if false, Error
  m_bOverFlowed = false;     // set to true if the buffer size failed
  m_nMaxSize = NET_MAXMESSAGE;
  m_nPushedCount = 0;
  m_bPushed = false;
  m_nReadCount = 0;
  m_bBadRead = false;

  Clear();
}

CMsgBuffer::~CMsgBuffer() {}

// Purpose: Temporarily remember the read position so we can reset it
void CMsgBuffer::Push() {
  // ??? Allow multiple pushes without matching pops ???
  assert(!m_bPushed);

  m_nPushedCount = m_nReadCount;
  m_bPushed = true;
}

void CMsgBuffer::Pop() {
  assert(m_bPushed);

  m_nReadCount = m_nPushedCount;
  m_bPushed = false;
}

void CMsgBuffer::SetOverflow(bool allowed) { m_bAllowOverflow = allowed; }

int CMsgBuffer::GetMaxSize() { return m_nMaxSize; }

void *CMsgBuffer::GetData() { return m_rgData; }

int CMsgBuffer::GetCurSize() { return m_nCurSize; }

int CMsgBuffer::GetReadCount() { return m_nReadCount; }

// Purpose: data accessor
void CMsgBuffer::SetTime(float time) { m_fRecvTime = time; }

// Purpose: data accessor
float CMsgBuffer::GetTime() { return m_fRecvTime; }

// Purpose: data accessor
void CMsgBuffer::SetNetAddress(netadr_t &adr) { m_NetAddr = adr; }

// Purpose: data accessor
netadr_t &CMsgBuffer::GetNetAddress() { return m_NetAddr; }

void CMsgBuffer::WriteByte(int c) {
  unsigned char *buf = (unsigned char *)GetSpace(1);
  if (buf) {
    buf[0] = c;
  }
}

void CMsgBuffer::WriteShort(int c) {
  unsigned char *buf = (unsigned char *)GetSpace(2);
  if (buf) {
    buf[0] = c & 0xff;
    buf[1] = c >> 8;
  }
}

void CMsgBuffer::WriteLong(int c) {
  unsigned char *buf = (unsigned char *)GetSpace(4);
  if (buf) {
    buf[0] = c & 0xff;
    buf[1] = (c >> 8) & 0xff;
    buf[2] = (c >> 16) & 0xff;
    buf[3] = c >> 24;
  }
}

void CMsgBuffer::WriteFloat(float f) {
  union {
    float f;
    int l;
  } dat;

  dat.f = f;
  Write(&dat.l, 4);
}

void CMsgBuffer::WriteString(const char *s) {
  if (!s) {
    Write("", 1);
  } else {
    Write(s, strlen(s) + 1);
  }
}

void CMsgBuffer::WriteBuf(int iSize, void *buf) {
  if (!buf) {
    return;
  }

  Write(buf, iSize);
}

void CMsgBuffer::BeginReading() {
  m_nReadCount = 0;
  m_bBadRead = false;
}

int CMsgBuffer::ReadByte() {
  int c;

  if (m_nReadCount + 1 > m_nCurSize) {
    m_bBadRead = true;
    return -1;
  }

  c = (unsigned char)m_rgData[m_nReadCount];
  m_nReadCount++;
  return c;
}

int CMsgBuffer::ReadShort() {
  int c;

  if (m_nReadCount + 2 > m_nCurSize) {
    m_bBadRead = true;
    return -1;
  }

  c = (short)(m_rgData[m_nReadCount] + (m_rgData[m_nReadCount + 1] << 8));
  m_nReadCount += 2;

  return c;
}

int CMsgBuffer::ReadLong() {
  int c;

  if (m_nReadCount + 4 > m_nCurSize) {
    m_bBadRead = true;
    return -1;
  }

  c = m_rgData[m_nReadCount] + (m_rgData[m_nReadCount + 1] << 8) +
      (m_rgData[m_nReadCount + 2] << 16) + (m_rgData[m_nReadCount + 3] << 24);

  m_nReadCount += 4;

  return c;
}

float CMsgBuffer::ReadFloat() {
  union {
    unsigned char b[4];
    float f;
  } dat;

  dat.b[0] = m_rgData[m_nReadCount];
  dat.b[1] = m_rgData[m_nReadCount + 1];
  dat.b[2] = m_rgData[m_nReadCount + 2];
  dat.b[3] = m_rgData[m_nReadCount + 3];
  m_nReadCount += 4;
  return dat.f;
}

int CMsgBuffer::ReadBuf(int iSize, void *pbuf) {
  if (m_nReadCount + iSize > m_nCurSize) {
    m_bBadRead = true;
    return -1;
  }

  memcpy(pbuf, &m_rgData[m_nReadCount], iSize);
  m_nReadCount += iSize;

  return 1;
}

char *CMsgBuffer::ReadString() {
  static char string[NET_MAXMESSAGE];
  int l, c;

  l = 0;
  do {
    c = (char)ReadByte();
    if (c == -1 || c == 0) break;
    string[l] = c;
    l++;
  } while (l < sizeof(string) - 1);

  string[l] = 0;

  return string;
}

void CMsgBuffer::Clear() {
  m_nCurSize = 0;
  m_bOverFlowed = false;
  m_nReadCount = 0;
  m_bBadRead = false;
  memset(m_rgData, 0, sizeof(m_rgData));
}

void *CMsgBuffer::GetSpace(int length) {
  void *d;

  if (m_nCurSize + length > m_nMaxSize) {
    if (!m_bAllowOverflow) {
      if (m_pfnErrorFunc) {
        (*m_pfnErrorFunc)(
            "CMsgBuffer(%s), no room for %i bytes, %i / %i already in use\n",
            m_pszBufferName, length, m_nCurSize, m_nMaxSize);
      }
      return NULL;
    }

    if (length > m_nMaxSize) {
      if (m_pfnErrorFunc) {
        (*m_pfnErrorFunc)("CMsgBuffer(%s), no room for %i bytes, %i is max\n",
                          m_pszBufferName, length, m_nMaxSize);
      }
      return NULL;
    }

    m_bOverFlowed = true;
    Clear();
  }

  d = m_rgData + m_nCurSize;
  m_nCurSize += length;
  return d;
}

void CMsgBuffer::Write(const void *data, int length) {
  void *space = GetSpace(length);
  if (space) {
    memcpy(space, data, length);
  }
}
