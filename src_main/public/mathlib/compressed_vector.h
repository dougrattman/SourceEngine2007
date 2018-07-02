// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_COMPRESSED_VECTOR_H_
#define SOURCE_MATHLIB_COMPRESSED_VECTOR_H_

#ifdef _WIN32
#pragma once
#endif

#include <cfloat>
#include <cmath>
#include <cstdlib>  // std::rand

#include "base/include/base_types.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier0/include/dbg.h"

// 32 bit vector.
class Vector32 {
 public:
  Vector32();
  Vector32(f32 X, f32 Y, f32 Z);

  Vector32 &operator=(const Vector &vOther);

  operator Vector() const {
    f32 fexp = expScale[exp] / 512.0f;

    return Vector{(((int)x) - 512) * fexp, (((int)y) - 512) * fexp,
                  (((int)z) - 512) * fexp};
  }

 private:
  u16 x : 10;
  u16 y : 10;
  u16 z : 10;
  u16 exp : 2;

  static constexpr f32 expScale[4] = {4.0f, 16.0f, 32.f, 64.f};
};

inline Vector32 &Vector32::operator=(const Vector &vOther) {
  CHECK_VALID(vOther);

  f32 fmax = std::max(fabs(vOther.x), fabs(vOther.y));
  fmax = std::max(fmax, fabs(vOther.z));

  if (fmax < expScale[0])
    exp = 0;
  else if (fmax < expScale[1])
    exp = 1;
  else if (fmax < expScale[2])
    exp = 2;
  else if (fmax < expScale[3])
    exp = 3;

  Assert(fmax < expScale[exp]);

  f32 fexp = 512.0f / expScale[exp];

  x = std::clamp((int)(vOther.x * fexp) + 512, 0, 1023);
  y = std::clamp((int)(vOther.y * fexp) + 512, 0, 1023);
  z = std::clamp((int)(vOther.z * fexp) + 512, 0, 1023);
  return *this;
}

// Unit 32 bits vector.
class Normal32 {
 public:
  // Construction/destruction:
  Normal32();
  Normal32(f32 X, f32 Y, f32 Z);

  // assignment
  Normal32 &operator=(const Vector &vOther);

  operator Vector() const {
    Vector tmp;
    tmp.x = ((int)x - 16384) * (1 / 16384.0);
    tmp.y = ((int)y - 16384) * (1 / 16384.0);
    tmp.z = sqrt(1 - tmp.x * tmp.x - tmp.y * tmp.y);

    if (zneg) tmp.z = -tmp.z;

    return tmp;
  }

 private:
  u16 x : 15;
  u16 y : 15;
  u16 zneg : 1;
};

inline Normal32 &Normal32::operator=(const Vector &vOther) {
  CHECK_VALID(vOther);

  x = std::clamp((int)(vOther.x * 16384) + 16384, 0, 32767);
  y = std::clamp((int)(vOther.y * 16384) + 16384, 0, 32767);
  zneg = (vOther.z < 0);
  return *this;
}

// 64 bit Quaternion.
class Quaternion64 {
 public:
  Quaternion64();
  Quaternion64(f32 X, f32 Y, f32 Z);

  Quaternion64 &operator=(const Quaternion &vOther);

  operator Quaternion() const {
    Quaternion tmp;

    // shift to -1048576, + 1048575, then round down slightly to -1.0 < x < 1.0
    tmp.x = ((int)x - 1048576) * (1 / 1048576.5f);
    tmp.y = ((int)y - 1048576) * (1 / 1048576.5f);
    tmp.z = ((int)z - 1048576) * (1 / 1048576.5f);
    tmp.w = sqrt(1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z);
    if (wneg) tmp.w = -tmp.w;
    return tmp;
  }

 private:
  u64 x : 21;
  u64 y : 21;
  u64 z : 21;
  u64 wneg : 1;
};

inline Quaternion64 &Quaternion64::operator=(const Quaternion &vOther) {
  CHECK_VALID(vOther);

  x = std::clamp((int)(vOther.x * 1048576) + 1048576, 0, 2097151);
  y = std::clamp((int)(vOther.y * 1048576) + 1048576, 0, 2097151);
  z = std::clamp((int)(vOther.z * 1048576) + 1048576, 0, 2097151);
  wneg = (vOther.w < 0);
  return *this;
}

// 48 bit Quaternion.
class Quaternion48 {
 public:
  Quaternion48();
  Quaternion48(f32 X, f32 Y, f32 Z);

  Quaternion48 &operator=(const Quaternion &vOther);

  operator Quaternion() const {
    Quaternion tmp;

    tmp.x = ((int)x - 32768) * (1 / 32768.0);
    tmp.y = ((int)y - 32768) * (1 / 32768.0);
    tmp.z = ((int)z - 16384) * (1 / 16384.0);
    tmp.w = sqrt(1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z);
    if (wneg) tmp.w = -tmp.w;
    return tmp;
  }

 private:
  u16 x : 16;
  u16 y : 16;
  u16 z : 15;
  u16 wneg : 1;
};

inline Quaternion48 &Quaternion48::operator=(const Quaternion &vOther) {
  CHECK_VALID(vOther);

  x = std::clamp((int)(vOther.x * 32768) + 32768, 0, 65535);
  y = std::clamp((int)(vOther.y * 32768) + 32768, 0, 65535);
  z = std::clamp((int)(vOther.z * 16384) + 16384, 0, 32767);
  wneg = (vOther.w < 0);
  return *this;
}

// 32 bit Quaternion.
class Quaternion32 {
 public:
  Quaternion32();
  Quaternion32(f32 X, f32 Y, f32 Z);

  Quaternion32 &operator=(const Quaternion &vOther);

  operator Quaternion() const {
    Quaternion tmp;

    tmp.x = ((int)x - 1024) * (1 / 1024.0);
    tmp.y = ((int)y - 512) * (1 / 512.0);
    tmp.z = ((int)z - 512) * (1 / 512.0);
    tmp.w = sqrt(1 - tmp.x * tmp.x - tmp.y * tmp.y - tmp.z * tmp.z);
    if (wneg) tmp.w = -tmp.w;
    return tmp;
  }

 private:
  u32 x : 11;
  u32 y : 10;
  u32 z : 10;
  u32 wneg : 1;
};

inline Quaternion32 &Quaternion32::operator=(const Quaternion &vOther) {
  CHECK_VALID(vOther);

  x = std::clamp((int)(vOther.x * 1024) + 1024, 0, 2047);
  y = std::clamp((int)(vOther.y * 512) + 512, 0, 1023);
  z = std::clamp((int)(vOther.z * 512) + 512, 0, 1023);
  wneg = (vOther.w < 0);
  return *this;
}

constexpr i32 float32bias = 127;
constexpr i32 float16bias = 15;
constexpr f32 maxfloat16bits = 65504.0f;

// 16 bit f32.
class float16 {
 public:
  float16() = default;
  float16(f32 f) { SetFloat(f); }

  void Init() { m_storage.rawWord = 0; }

  u16 GetBits() const { return m_storage.rawWord; }

  f32 GetFloat() const { return Convert16bitFloatTo32bits(m_storage.rawWord); }

  void SetFloat(f32 in) { m_storage.rawWord = ConvertFloatTo16bits(in); }

  bool IsInfinity() const {
    return m_storage.bits.biased_exponent == 31 && m_storage.bits.mantissa == 0;
  }

  bool IsNaN() const {
    return m_storage.bits.biased_exponent == 31 && m_storage.bits.mantissa != 0;
  }

  bool operator==(const float16 other) const {
    return m_storage.rawWord == other.m_storage.rawWord;
  }

  bool operator!=(const float16 other) const {
    return m_storage.rawWord != other.m_storage.rawWord;
  }

 protected:
  union float32bits {
    f32 rawFloat;
    struct {
      u32 mantissa : 23;
      u32 biased_exponent : 8;
      u32 sign : 1;
    } bits;
  };

  union float16bits {
    u16 rawWord;
    struct {
      u16 mantissa : 10;
      u16 biased_exponent : 5;
      u16 sign : 1;
    } bits;
  };

  static constexpr bool IsNaN(float16bits in) {
    return in.bits.biased_exponent == 31 && in.bits.mantissa != 0;
  }

  static constexpr bool IsInfinity(float16bits in) {
    return in.bits.biased_exponent == 31 && in.bits.mantissa == 0;
  }

  // 0x0001 - 0x03ff
  static u16 ConvertFloatTo16bits(f32 in) {
    if (in > maxfloat16bits)
      in = maxfloat16bits;
    else if (in < -maxfloat16bits)
      in = -maxfloat16bits;

    float32bits inFloat;
    inFloat.rawFloat = in;

    float16bits output;
    output.bits.sign = inFloat.bits.sign;

    if ((inFloat.bits.biased_exponent == 0) && (inFloat.bits.mantissa == 0)) {
      // zero
      output.bits.mantissa = 0;
      output.bits.biased_exponent = 0;
    } else if ((inFloat.bits.biased_exponent == 0) &&
               (inFloat.bits.mantissa != 0)) {
      // denorm -- denorm f32 maps to 0 half
      output.bits.mantissa = 0;
      output.bits.biased_exponent = 0;
    } else if ((inFloat.bits.biased_exponent == 0xff) &&
               (inFloat.bits.mantissa == 0)) {
      // infinity maps to maxfloat
      output.bits.mantissa = 0x3ff;
      output.bits.biased_exponent = 0x1e;
    } else if ((inFloat.bits.biased_exponent == 0xff) &&
               (inFloat.bits.mantissa != 0)) {
      // NaN maps to zero
      output.bits.mantissa = 0;
      output.bits.biased_exponent = 0;
    } else {
      // regular number
      int new_exp = inFloat.bits.biased_exponent - 127;

      if (new_exp < -24) {
        // this maps to 0
        output.bits.mantissa = 0;
        output.bits.biased_exponent = 0;
      }

      if (new_exp < -14) {
        // this maps to a denorm
        output.bits.biased_exponent = 0;
        u32 exp_val = (u32)(-14 - (inFloat.bits.biased_exponent - float32bias));
        if (exp_val > 0 && exp_val < 11) {
          output.bits.mantissa =
              (1 << (10 - exp_val)) + (inFloat.bits.mantissa >> (13 + exp_val));
        }
      } else if (new_exp > 15) {
        // to big. . . maps to maxfloat
        output.bits.mantissa = 0x3ff;
        output.bits.biased_exponent = 0x1e;
      } else {
        output.bits.biased_exponent = new_exp + 15;
        output.bits.mantissa = (inFloat.bits.mantissa >> 13);
      }
    }
    return output.rawWord;
  }

  static f32 Convert16bitFloatTo32bits(u16 in) {
    const float16bits &inFloat = *((float16bits *)&in);

    if (IsInfinity(inFloat)) {
      return maxfloat16bits * ((inFloat.bits.sign == 1) ? -1.0f : 1.0f);
    }

    if (IsNaN(inFloat)) return 0.0;

    float32bits output;
    if (inFloat.bits.biased_exponent == 0 && inFloat.bits.mantissa != 0) {
      // denorm
      const f32 half_denorm = (1.0f / 16384.0f);  // 2^-14
      f32 mantissa = ((f32)(inFloat.bits.mantissa)) / 1024.0f;
      f32 sgn = (inFloat.bits.sign) ? -1.0f : 1.0f;
      output.rawFloat = sgn * mantissa * half_denorm;
    } else {
      // regular number
      u32 mantissa = inFloat.bits.mantissa;
      u32 biased_exponent = inFloat.bits.biased_exponent;
      u32 sign = ((u32)inFloat.bits.sign) << 31;
      biased_exponent = ((biased_exponent - float16bias + float32bias) *
                         (biased_exponent != 0))
                        << 23;
      mantissa <<= (23 - 10);

      *((u32 *)&output) = (mantissa | biased_exponent | sign);
    }

    return output.rawFloat;
  }

  float16bits m_storage;
};

class float16_with_assign : public float16 {
 public:
  float16_with_assign() {}
  float16_with_assign(f32 f) { m_storage.rawWord = ConvertFloatTo16bits(f); }

  float16 &operator=(const float16 &other) {
    m_storage.rawWord = ((float16_with_assign &)other).m_storage.rawWord;
    return *this;
  }

  float16 &operator=(const f32 &other) {
    m_storage.rawWord = ConvertFloatTo16bits(other);
    return *this;
  }

  operator f32() const { return Convert16bitFloatTo32bits(m_storage.rawWord); }
};

// 48 bit 3D vector.
class Vector48 {
 public:
  Vector48() {}
  Vector48(f32 X, f32 Y, f32 Z) {
    x.SetFloat(X);
    y.SetFloat(Y);
    z.SetFloat(Z);
  }

  Vector48 &operator=(const Vector &vOther);

  operator Vector() const {
    return Vector{x.GetFloat(), y.GetFloat(), z.GetFloat()};
  }

  const f32 operator[](int i) const {
    return (((float16 *)this)[i]).GetFloat();
  }

  float16 x;
  float16 y;
  float16 z;
};

inline Vector48 &Vector48::operator=(const Vector &vOther) {
  CHECK_VALID(vOther);

  x.SetFloat(vOther.x);
  y.SetFloat(vOther.y);
  z.SetFloat(vOther.z);
  return *this;
}

// 32 bit 2D vector.
class Vector2d32 {
 public:
  Vector2d32() {}
  Vector2d32(f32 X, f32 Y) {
    x.SetFloat(X);
    y.SetFloat(Y);
  }

  Vector2d32 &operator=(const Vector &vOther);
  Vector2d32 &operator=(const Vector2D &vOther);

  operator Vector2D() const { return Vector2D{x.GetFloat(), y.GetFloat()}; }

  void Init(f32 ix = 0.f, f32 iy = 0.f) {
    x.SetFloat(ix);
    y.SetFloat(iy);
  }

  float16_with_assign x;
  float16_with_assign y;
};

inline Vector2d32 &Vector2d32::operator=(const Vector2D &vOther) {
  Init(vOther.x, vOther.y);
  return *this;
}

#endif  // SOURCE_MATHLIB_COMPRESSED_VECTOR_H_
