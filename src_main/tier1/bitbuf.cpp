// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier1/bitbuf.h"

#include "bitvec.h"
#include "coordsize.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier1/strtools.h"

// TODO(d.rattman): Can't use this until we get multithreaded allocations in
// tier0 working for tools This is used by VVIS and fails to link NOTE: This
// must be the last file included!!!
//#include "tier0/include/memdbgon.h"

#ifdef OS_WIN
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)

inline u32 CountLeadingZeros(u32 x) {
  unsigned long firstBit;
  if (_BitScanReverse(&firstBit, x)) return 31 - firstBit;
  return 32;
}
inline u32 CountTrailingZeros(u32 elem) {
  unsigned long out;
  if (_BitScanForward(&out, elem)) return out;
  return 32;
}

#define FAST_BIT_SCAN 1
#else
#define FAST_BIT_SCAN 0
#endif

static BitBufErrorHandler g_BitBufErrorHandler{nullptr};

inline int BitForBitnum(int bitnum) { return GetBitForBitnum(bitnum); }

void InternalBitBufErrorHandler(BitBufErrorType errorType,
                                const char *pDebugName) {
  if (g_BitBufErrorHandler) g_BitBufErrorHandler(errorType, pDebugName);
}

void SetBitBufErrorHandler(BitBufErrorHandler fn) { g_BitBufErrorHandler = fn; }

// #define BB_PROFILING

// Precalculated bit masks for WriteUBitLong. Using these tables instead of
// doing the calculations gives a 33% speedup in WriteUBitLong.
unsigned long g_BitWriteMasks[32][33];

// (1 << i) - 1
unsigned long g_ExtraMasks[32];

struct BitWriteMasksInit {
  BitWriteMasksInit() {
    for (u32 startbit = 0; startbit < 32; startbit++) {
      for (u32 nBitsLeft = 0; nBitsLeft < 33; nBitsLeft++) {
        const u32 endbit = startbit + nBitsLeft;

        g_BitWriteMasks[startbit][nBitsLeft] = BitForBitnum(startbit) - 1;

        if (endbit < 32)
          g_BitWriteMasks[startbit][nBitsLeft] |= ~(BitForBitnum(endbit) - 1);
      }
    }

    for (u32 maskBit = 0; maskBit < 32; maskBit++)
      g_ExtraMasks[maskBit] = BitForBitnum(maskBit) - 1;
  }
};

BitWriteMasksInit g_BitWriteMasksInit;

// old_bf_write
old_bf_write::old_bf_write() {
  m_pData = nullptr;
  m_nDataBytes = 0;
  m_nDataBits = -1;  // set to -1 so we generate overflow on any operation
  m_iCurBit = 0;
  m_bOverflow = false;
  m_bAssertOnOverflow = true;
  m_pDebugName = nullptr;
}

old_bf_write::old_bf_write(const char *pDebugName, void *pData, int nBytes,
                           int nBits) {
  m_bAssertOnOverflow = true;
  m_pDebugName = pDebugName;
  StartWriting(pData, nBytes, 0, nBits);
}

old_bf_write::old_bf_write(void *pData, int nBytes, int nBits) {
  m_bAssertOnOverflow = true;
  m_pDebugName = nullptr;
  StartWriting(pData, nBytes, 0, nBits);
}

void old_bf_write::StartWriting(void *pData, int nBytes, int iStartBit,
                                int nBits) {
  // Make sure it's dword aligned and padded.
  Assert((nBytes % 4) == 0);
  Assert(((unsigned long)pData & 3) == 0);

  // The writing code will overrun the end of the buffer if it isn't dword
  // aligned, so truncate to force alignment
  nBytes &= ~3;

  m_pData = (u8 *)pData;
  m_nDataBytes = nBytes;

  if (nBits == -1) {
    m_nDataBits = nBytes << 3;
  } else {
    Assert(nBits <= nBytes * 8);
    m_nDataBits = nBits;
  }

  m_iCurBit = iStartBit;
  m_bOverflow = false;
}

void old_bf_write::Reset() {
  m_iCurBit = 0;
  m_bOverflow = false;
}

void old_bf_write::SetAssertOnOverflow(bool bAssert) {
  m_bAssertOnOverflow = bAssert;
}

const char *old_bf_write::GetDebugName() { return m_pDebugName; }

void old_bf_write::SetDebugName(const char *pDebugName) {
  m_pDebugName = pDebugName;
}

void old_bf_write::SeekToBit(int bitPos) { m_iCurBit = bitPos; }

// Sign bit comes first
void old_bf_write::WriteSBitLong(int data, int numbits) {
  // Do we have a valid # of bits to encode with?
  Assert(numbits >= 1);

  // Note: it does this wierdness here so it's bit-compatible with regular
  // integer data in the buffer. (Some old code writes direct integers right
  // into the buffer).
  if (data < 0) {
#ifndef NDEBUG
    if (numbits < 32) {
      // Make sure it doesn't overflow.

      if (data < 0) {
        Assert(data >= -(BitForBitnum(numbits - 1)));
      } else {
        Assert(data < (BitForBitnum(numbits - 1)));
      }
    }
#endif

    WriteUBitLong((u32)(0x80000000 + data), numbits - 1, false);
    WriteOneBit(1);
  } else {
    WriteUBitLong((u32)data, numbits - 1);
    WriteOneBit(0);
  }
}

#ifdef OS_WIN
inline u32 BitCountNeededToEncode(u32 data) {
  unsigned long firstBit;
  _BitScanReverse(&firstBit, data + 1);
  return firstBit;
}
#endif  // OS_WIN

// writes an unsigned integer with variable bit length
void old_bf_write::WriteUBitVar(u32 data) {
  if ((data & 0xf) == data) {
    WriteUBitLong(0, 2);
    WriteUBitLong(data, 4);
  } else {
    if ((data & 0xff) == data) {
      WriteUBitLong(1, 2);
      WriteUBitLong(data, 8);
    } else {
      if ((data & 0xfff) == data) {
        WriteUBitLong(2, 2);
        WriteUBitLong(data, 12);
      } else {
        WriteUBitLong(0x3, 2);
        WriteUBitLong(data, 32);
      }
    }
  }
}

void old_bf_write::WriteBitLong(u32 data, int numbits, bool bSigned) {
  if (bSigned)
    WriteSBitLong((int)data, numbits);
  else
    WriteUBitLong(data, numbits);
}

bool old_bf_write::WriteBits(const void *pInData, int nBits) {
#ifdef BB_PROFILING
  VPROF("old_bf_write::WriteBits");
#endif

  u8 *pOut = (u8 *)pInData;
  int nBitsLeft = nBits;

  // Bounds checking..
  if ((m_iCurBit + nBits) > m_nDataBits) {
    SetOverflowFlag();
    CallErrorHandler(BITBUFERROR_BUFFER_OVERRUN, GetDebugName());
    return false;
  }

  // Align output to dword boundary
  while (((uintptr_t)pOut & 3) != 0 && nBitsLeft >= 8) {
    WriteUBitLong(*pOut, 8, false);
    ++pOut;
    nBitsLeft -= 8;
  }

  if ((nBitsLeft >= 32) && (m_iCurBit & 7) == 0) {
    // current bit is byte aligned, do block copy
    int numbytes = nBitsLeft >> 3;
    int numbits = numbytes << 3;

    Q_memcpy(m_pData + (m_iCurBit >> 3), pOut, numbytes);
    pOut += numbytes;
    nBitsLeft -= numbits;
    m_iCurBit += numbits;
  }

  if (nBitsLeft >= 32) {
    unsigned long iBitsRight = (m_iCurBit & 31);
    unsigned long iBitsLeft = 32 - iBitsRight;
    unsigned long bitMaskLeft = g_BitWriteMasks[iBitsRight][32];
    unsigned long bitMaskRight = g_BitWriteMasks[0][iBitsRight];

    unsigned long *pData = &((unsigned long *)m_pData)[m_iCurBit >> 5];

    // Read dwords.
    while (nBitsLeft >= 32) {
      unsigned long curData = *(unsigned long *)pOut;
      pOut += sizeof(unsigned long);

      *pData &= bitMaskLeft;
      *pData |= curData << iBitsRight;

      pData++;

      if (iBitsLeft < 32) {
        curData >>= iBitsLeft;
        *pData &= bitMaskRight;
        *pData |= curData;
      }

      nBitsLeft -= 32;
      m_iCurBit += 32;
    }
  }

  // write remaining bytes
  while (nBitsLeft >= 8) {
    WriteUBitLong(*pOut, 8, false);
    ++pOut;
    nBitsLeft -= 8;
  }

  // write remaining bits
  if (nBitsLeft) {
    WriteUBitLong(*pOut, nBitsLeft, false);
  }

  return !IsOverflowed();
}

bool old_bf_write::WriteBitsFromBuffer(bf_read *pIn, int nBits) {
  // This could be optimized a little by
  while (nBits > 32) {
    WriteUBitLong(pIn->ReadUBitLong(32), 32);
    nBits -= 32;
  }

  WriteUBitLong(pIn->ReadUBitLong(nBits), nBits);
  return !IsOverflowed() && !pIn->IsOverflowed();
}

void old_bf_write::WriteBitAngle(f32 fAngle, int numbits) {
  u32 shift = BitForBitnum(numbits);
  u32 mask = shift - 1;

  int d = (int)((fAngle / 360.0f) * shift);
  d &= mask;

  WriteUBitLong((u32)d, numbits);
}

void old_bf_write::WriteBitCoordMP(const f32 f, bool bIntegral,
                                   bool bLowPrecision) {
#ifdef BB_PROFILING
  VPROF("old_bf_write::WriteBitCoordMP");
#endif
  int signbit = (f <= -(bLowPrecision ? COORD_RESOLUTION_LOWPRECISION
                                      : COORD_RESOLUTION));
  int intval = (int)abs(f);
  int fractval =
      bLowPrecision
          ? (abs((int)(f * COORD_DENOMINATOR_LOWPRECISION)) &
             (COORD_DENOMINATOR_LOWPRECISION - 1))
          : (abs((int)(f * COORD_DENOMINATOR)) & (COORD_DENOMINATOR - 1));

  bool bInBounds = intval < (1 << COORD_INTEGER_BITS_MP);

  WriteOneBit(bInBounds);

  if (bIntegral) {
    // Send the sign bit
    WriteOneBit(intval);
    if (intval) {
      WriteOneBit(signbit);
      // Send the integer if we have one.
      // Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
      intval--;
      if (bInBounds) {
        WriteUBitLong((u32)intval, COORD_INTEGER_BITS_MP);
      } else {
        WriteUBitLong((u32)intval, COORD_INTEGER_BITS);
      }
    }
  } else {
    // Send the bit flags that indicate whether we have an integer part and/or a
    // fraction part.
    WriteOneBit(intval);
    // Send the sign bit
    WriteOneBit(signbit);

    if (intval) {
      // Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
      intval--;
      if (bInBounds) {
        WriteUBitLong((u32)intval, COORD_INTEGER_BITS_MP);
      } else {
        WriteUBitLong((u32)intval, COORD_INTEGER_BITS);
      }
    }
    WriteUBitLong((u32)fractval, bLowPrecision
                                     ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION
                                     : COORD_FRACTIONAL_BITS);
  }
}

void old_bf_write::WriteBitCoord(const f32 f) {
#ifdef BB_PROFILING
  VPROF("old_bf_write::WriteBitCoord");
#endif
  int signbit = (f <= -COORD_RESOLUTION);
  int intval = (int)abs(f);
  int fractval = abs((int)(f * COORD_DENOMINATOR)) & (COORD_DENOMINATOR - 1);

  // Send the bit flags that indicate whether we have an integer part and/or a
  // fraction part.
  WriteOneBit(intval);
  WriteOneBit(fractval);

  if (intval || fractval) {
    // Send the sign bit
    WriteOneBit(signbit);

    // Send the integer if we have one.
    if (intval) {
      // Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
      intval--;
      WriteUBitLong((u32)intval, COORD_INTEGER_BITS);
    }

    // Send the fraction if we have one
    if (fractval) {
      WriteUBitLong((u32)fractval, COORD_FRACTIONAL_BITS);
    }
  }
}

void old_bf_write::WriteBitFloat(f32 val) {
  static_assert(sizeof(long) == sizeof(f32));
  static_assert(sizeof(f32) == 4);

  long intVal = *((long *)&val);
  WriteUBitLong(intVal, 32);
}

void old_bf_write::WriteBitVec3Coord(const Vector &fa) {
  int xflag = (fa[0] >= COORD_RESOLUTION) || (fa[0] <= -COORD_RESOLUTION);
  int yflag = (fa[1] >= COORD_RESOLUTION) || (fa[1] <= -COORD_RESOLUTION);
  int zflag = (fa[2] >= COORD_RESOLUTION) || (fa[2] <= -COORD_RESOLUTION);

  WriteOneBit(xflag);
  WriteOneBit(yflag);
  WriteOneBit(zflag);

  if (xflag) WriteBitCoord(fa[0]);
  if (yflag) WriteBitCoord(fa[1]);
  if (zflag) WriteBitCoord(fa[2]);
}

void old_bf_write::WriteBitNormal(f32 f) {
  int signbit = (f <= -NORMAL_RESOLUTION);

  // NOTE: Since +/-1 are valid values for a normal, I'm going to encode that as
  // all ones
  u32 fractval = abs((int)(f * NORMAL_DENOMINATOR));

  // clamp..
  if (fractval > NORMAL_DENOMINATOR) fractval = NORMAL_DENOMINATOR;

  // Send the sign bit
  WriteOneBit(signbit);

  // Send the fractional component
  WriteUBitLong(fractval, NORMAL_FRACTIONAL_BITS);
}

void old_bf_write::WriteBitVec3Normal(const Vector &fa) {
  int xflag = (fa[0] >= NORMAL_RESOLUTION) || (fa[0] <= -NORMAL_RESOLUTION);
  int yflag = (fa[1] >= NORMAL_RESOLUTION) || (fa[1] <= -NORMAL_RESOLUTION);

  WriteOneBit(xflag);
  WriteOneBit(yflag);

  if (xflag) WriteBitNormal(fa[0]);
  if (yflag) WriteBitNormal(fa[1]);

  // Write z sign bit
  int signbit = (fa[2] <= -NORMAL_RESOLUTION);
  WriteOneBit(signbit);
}

void old_bf_write::WriteBitAngles(const QAngle &fa) {
  WriteBitVec3Coord(Vector{fa.x, fa.y, fa.z});
}

void old_bf_write::WriteChar(int val) { WriteSBitLong(val, sizeof(char) << 3); }

void old_bf_write::WriteByte(int val) { WriteUBitLong(val, sizeof(u8) << 3); }

void old_bf_write::WriteShort(int val) { WriteSBitLong(val, sizeof(i16) << 3); }

void old_bf_write::WriteWord(int val) { WriteUBitLong(val, sizeof(u16) << 3); }

void old_bf_write::WriteLong(long val) {
  WriteSBitLong(val, sizeof(long) << 3);
}

void old_bf_write::WriteLongLong(int64_t val) {
  u32 *pLongs = (u32 *)&val;

  // Insert the two DWORDS according to network endian
  const i16 endianIndex = 0x0100;
  u8 *idx = (u8 *)&endianIndex;
  WriteUBitLong(pLongs[*idx++], sizeof(long) << 3);
  WriteUBitLong(pLongs[*idx], sizeof(long) << 3);
}

void old_bf_write::WriteFloat(f32 val) {
  // Pre-swap the f32, since WriteBits writes raw data
  LittleFloat(&val, &val);

  WriteBits(&val, sizeof(val) << 3);
}

bool old_bf_write::WriteBytes(const void *pBuf, int nBytes) {
  return WriteBits(pBuf, nBytes << 3);
}

bool old_bf_write::WriteString(const char *pStr) {
  if (pStr) {
    do {
      WriteChar(*pStr);
      ++pStr;
    } while (*(pStr - 1) != 0);
  } else {
    WriteChar(0);
  }

  return !IsOverflowed();
}

// old_bf_read
old_bf_read::old_bf_read() {
  m_pData = nullptr;
  m_nDataBytes = 0;
  m_nDataBits = -1;  // set to -1 so we overflow on any operation
  m_iCurBit = 0;
  m_bOverflow = false;
  m_bAssertOnOverflow = true;
  m_pDebugName = nullptr;
}

old_bf_read::old_bf_read(const void *pData, int nBytes, int nBits) {
  m_bAssertOnOverflow = true;
  StartReading(pData, nBytes, 0, nBits);
}

old_bf_read::old_bf_read(const char *pDebugName, const void *pData, int nBytes,
                         int nBits) {
  m_bAssertOnOverflow = true;
  m_pDebugName = pDebugName;
  StartReading(pData, nBytes, 0, nBits);
}

void old_bf_read::StartReading(const void *pData, int nBytes, int iStartBit,
                               int nBits) {
  // Make sure we're dword aligned.
  Assert(((unsigned long)pData & 3) == 0);

  m_pData = (u8 *)pData;
  m_nDataBytes = nBytes;

  if (nBits == -1) {
    m_nDataBits = m_nDataBytes << 3;
  } else {
    Assert(nBits <= nBytes * 8);
    m_nDataBits = nBits;
  }

  m_iCurBit = iStartBit;
  m_bOverflow = false;
}

void old_bf_read::Reset() {
  m_iCurBit = 0;
  m_bOverflow = false;
}

void old_bf_read::SetAssertOnOverflow(bool bAssert) {
  m_bAssertOnOverflow = bAssert;
}

const char *old_bf_read::GetDebugName() { return m_pDebugName; }

void old_bf_read::SetDebugName(const char *pName) { m_pDebugName = pName; }

u32 old_bf_read::CheckReadUBitLong(int numbits) {
  // Ok, just read bits out.
  int i, nBitValue;
  u32 r = 0;

  for (i = 0; i < numbits; i++) {
    nBitValue = ReadOneBitNoCheck();
    r |= nBitValue << i;
  }
  m_iCurBit -= numbits;

  return r;
}

void old_bf_read::ReadBits(void *pOutData, int nBits) {
#ifdef BB_PROFILING
  VPROF("old_bf_write::ReadBits");
#endif

  u8 *pOut = (u8 *)pOutData;
  int nBitsLeft = nBits;

  // align output to dword boundary
  while (((uintptr_t)pOut & 3) != 0 && nBitsLeft >= 8) {
    *pOut = (u8)ReadUBitLong(8);
    ++pOut;
    nBitsLeft -= 8;
  }

  // read dwords
  while (nBitsLeft >= 32) {
    *((unsigned long *)pOut) = ReadUBitLong(32);
    pOut += sizeof(unsigned long);
    nBitsLeft -= 32;
  }

  // read remaining bytes
  while (nBitsLeft >= 8) {
    *pOut = ReadUBitLong(8);
    ++pOut;
    nBitsLeft -= 8;
  }

  // read remaining bits
  if (nBitsLeft) {
    *pOut = ReadUBitLong(nBitsLeft);
  }
}

f32 old_bf_read::ReadBitAngle(int numbits) {
  f32 fReturn;
  int i;
  f32 shift;

  shift = (f32)(BitForBitnum(numbits));

  i = ReadUBitLong(numbits);
  fReturn = (f32)i * (360.0 / shift);

  return fReturn;
}

u32 old_bf_read::PeekUBitLong(int numbits) {
  u32 r;
  int i, nBitValue;
#ifdef BIT_VERBOSE
  int nShifts = numbits;
#endif

  old_bf_read savebf;

  savebf = *this;  // Save current state info

  r = 0;
  for (i = 0; i < numbits; i++) {
    nBitValue = ReadOneBit();

    // Append to current stream
    if (nBitValue) {
      r |= BitForBitnum(i);
    }
  }

  *this = savebf;

#ifdef BIT_VERBOSE
  Con_Printf("PeekBitLong:  %i %i\n", nShifts, (u32)r);
#endif

  return r;
}

// Append numbits least significant bits from data to the current bit stream
int old_bf_read::ReadSBitLong(int numbits) {
  int r = ReadUBitLong(numbits - 1);

  // Note: it does this wierdness here so it's bit-compatible with regular
  // integer data in the buffer. (Some old code writes direct integers right
  // into the buffer).
  int sign = ReadOneBit();
  if (sign) r = -((BitForBitnum(numbits - 1)) - r);

  return r;
}

const u8 g_BitMask[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
const u8 g_TrailingMask[8] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

inline int old_bf_read::CountRunOfZeros() {
  int bits = 0;
  if (m_iCurBit + 32 < m_nDataBits) {
#if !FAST_BIT_SCAN
    while (true) {
      int value = (m_pData[m_iCurBit >> 3] & g_BitMask[m_iCurBit & 7]);
      ++m_iCurBit;
      if (value) return bits;
      ++bits;
    }
#else
    while (true) {
      int value = (m_pData[m_iCurBit >> 3] & g_TrailingMask[m_iCurBit & 7]);
      if (!value) {
        int zeros = (8 - (m_iCurBit & 7));
        bits += zeros;
        m_iCurBit += zeros;
      } else {
        int zeros = CountTrailingZeros(value) - (m_iCurBit & 7);
        m_iCurBit += zeros + 1;
        bits += zeros;
        return bits;
      }
    }
#endif
  } else {
    while (ReadOneBit() == 0) bits++;
  }

  return bits;
}

u32 old_bf_read::ReadUBitVar() {
  switch (ReadUBitLong(2)) {
    case 0:
      return ReadUBitLong(4);

    case 1:
      return ReadUBitLong(8);

    case 2:
      return ReadUBitLong(12);

    default:
    case 3:
      return ReadUBitLong(32);
  }
}

u32 old_bf_read::ReadBitLong(int numbits, bool bSigned) {
  if (bSigned) return (u32)ReadSBitLong(numbits);

  return ReadUBitLong(numbits);
}

// Basic Coordinate Routines (these contain bit-field size AND fixed point
// scaling constants)
f32 old_bf_read::ReadBitCoord() {
#if defined(BB_PROFILING)
  VPROF("old_bf_write::ReadBitCoord");
#endif
  int intval = 0, fractval = 0, signbit = 0;
  f32 value = 0.0;

  // Read the required integer and fraction flags
  intval = ReadOneBit();
  fractval = ReadOneBit();

  // If we got either parse them, otherwise it's a zero.
  if (intval || fractval) {
    // Read the sign bit
    signbit = ReadOneBit();

    // If there's an integer, read it in
    if (intval) {
      // Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
      intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
    }

    // If there's a fraction, read it in
    if (fractval) {
      fractval = ReadUBitLong(COORD_FRACTIONAL_BITS);
    }

    // Calculate the correct floating point value
    value = intval + ((f32)fractval * COORD_RESOLUTION);

    // Fixup the sign if negative.
    if (signbit) value = -value;
  }

  return value;
}

f32 old_bf_read::ReadBitCoordMP(bool bIntegral, bool bLowPrecision) {
#if defined(BB_PROFILING)
  VPROF("old_bf_write::ReadBitCoordMP");
#endif
  int intval = 0, fractval = 0, signbit = 0;
  f32 value = 0.0;

  bool bInBounds = ReadOneBit() ? true : false;

  if (bIntegral) {
    // Read the required integer and fraction flags
    intval = ReadOneBit();
    // If we got either parse them, otherwise it's a zero.
    if (intval) {
      // Read the sign bit
      signbit = ReadOneBit();

      // If there's an integer, read it in
      // Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
      if (bInBounds) {
        value = ReadUBitLong(COORD_INTEGER_BITS_MP) + 1;
      } else {
        value = ReadUBitLong(COORD_INTEGER_BITS) + 1;
      }
    }
  } else {
    // Read the required integer and fraction flags
    intval = ReadOneBit();

    // Read the sign bit
    signbit = ReadOneBit();

    // If we got either parse them, otherwise it's a zero.
    if (intval) {
      if (bInBounds) {
        intval = ReadUBitLong(COORD_INTEGER_BITS_MP) + 1;
      } else {
        intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
      }
    }

    // If there's a fraction, read it in
    fractval =
        ReadUBitLong(bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION
                                   : COORD_FRACTIONAL_BITS);

    // Calculate the correct floating point value
    value =
        intval + ((f32)fractval * (bLowPrecision ? COORD_RESOLUTION_LOWPRECISION
                                                 : COORD_RESOLUTION));
  }

  // Fixup the sign if negative.
  if (signbit) value = -value;

  return value;
}

void old_bf_read::ReadBitVec3Coord(Vector &fa) {
  int xflag, yflag, zflag;

  // This vector must be initialized! Otherwise, If any of the flags aren't set,
  // the corresponding component will not be read and will be stack garbage.
  fa.Init(0, 0, 0);

  xflag = ReadOneBit();
  yflag = ReadOneBit();
  zflag = ReadOneBit();

  if (xflag) fa[0] = ReadBitCoord();
  if (yflag) fa[1] = ReadBitCoord();
  if (zflag) fa[2] = ReadBitCoord();
}

f32 old_bf_read::ReadBitNormal() {
  // Read the sign bit
  int signbit = ReadOneBit();

  // Read the fractional part
  u32 fractval = ReadUBitLong(NORMAL_FRACTIONAL_BITS);

  // Calculate the correct floating point value
  f32 value = (f32)fractval * NORMAL_RESOLUTION;

  // Fixup the sign if negative.
  if (signbit) value = -value;

  return value;
}

void old_bf_read::ReadBitVec3Normal(Vector &fa) {
  int xflag = ReadOneBit();
  int yflag = ReadOneBit();

  if (xflag)
    fa[0] = ReadBitNormal();
  else
    fa[0] = 0.0f;

  if (yflag)
    fa[1] = ReadBitNormal();
  else
    fa[1] = 0.0f;

  // The first two imply the third (but not its sign)
  int znegative = ReadOneBit();

  f32 fafafbfb = fa[0] * fa[0] + fa[1] * fa[1];
  if (fafafbfb < 1.0f)
    fa[2] = sqrt(1.0f - fafafbfb);
  else
    fa[2] = 0.0f;

  if (znegative) fa[2] = -fa[2];
}

void old_bf_read::ReadBitAngles(QAngle &fa) {
  Vector tmp;
  ReadBitVec3Coord(tmp);
  fa.Init(tmp.x, tmp.y, tmp.z);
}

int old_bf_read::ReadChar() { return ReadSBitLong(sizeof(char) << 3); }

int old_bf_read::ReadByte() { return ReadUBitLong(sizeof(u8) << 3); }

int old_bf_read::ReadShort() { return ReadSBitLong(sizeof(i16) << 3); }

int old_bf_read::ReadWord() { return ReadUBitLong(sizeof(u16) << 3); }

long old_bf_read::ReadLong() { return ReadSBitLong(sizeof(long) << 3); }

int64_t old_bf_read::ReadLongLong() {
  int64_t retval;
  u32 *pLongs = (u32 *)&retval;

  // Read the two DWORDs according to network endian
  const i16 endianIndex = 0x0100;
  u8 *idx = (u8 *)&endianIndex;
  pLongs[*idx++] = ReadUBitLong(sizeof(long) << 3);
  pLongs[*idx] = ReadUBitLong(sizeof(long) << 3);

  return retval;
}

f32 old_bf_read::ReadFloat() {
  f32 ret;
  static_assert(sizeof(ret) == 4);
  ReadBits(&ret, 32);

  // Swap the f32, since ReadBits reads raw data
  LittleFloat(&ret, &ret);
  return ret;
}

bool old_bf_read::ReadBytes(void *pOut, int nBytes) {
  ReadBits(pOut, nBytes << 3);
  return !IsOverflowed();
}

bool old_bf_read::ReadString(char *pStr, int maxLen, bool bLine,
                             int *pOutNumChars) {
  Assert(maxLen != 0);

  bool bTooSmall = false;
  int iChar = 0;
  while (1) {
    char val = ReadChar();
    if (val == 0)
      break;
    else if (bLine && val == '\n')
      break;

    if (iChar < (maxLen - 1)) {
      pStr[iChar] = val;
      ++iChar;
    } else {
      bTooSmall = true;
    }
  }

  // Make sure it's 0-terminated.
  Assert(iChar < maxLen);
  pStr[iChar] = 0;

  if (pOutNumChars) *pOutNumChars = iChar;

  return !IsOverflowed() && !bTooSmall;
}

char *old_bf_read::ReadAndAllocateString(bool *pOverflow) {
  char str[2048];

  int nChars;
  bool bOverflow = !ReadString(str, sizeof(str), false, &nChars);
  if (pOverflow) *pOverflow = bOverflow;

  // Now copy into the output and return it;
  char *pRet = new char[nChars + 1];
  for (int i = 0; i <= nChars; i++) pRet[i] = str[i];

  return pRet;
}

void old_bf_read::ExciseBits(int startbit, int bitstoremove) {
  int endbit = startbit + bitstoremove;
  int remaining_to_end = m_nDataBits - endbit;

  old_bf_write temp;
  temp.StartWriting((void *)m_pData, m_nDataBits << 3, startbit);

  Seek(endbit);

  for (int i = 0; i < remaining_to_end; i++) {
    temp.WriteOneBit(ReadOneBit());
  }

  Seek(startbit);

  m_nDataBits -= bitstoremove;
  m_nDataBytes = m_nDataBits >> 3;
}
