// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef COLOR_H
#define COLOR_H

#include <memory.h>
#include <type_traits>
#include "base/include/base_types.h"

// Basic handler for an rgb set of colors.
class Color {
 public:
  Color() { memset(this, 0, sizeof(*this)); }
  Color(u8 r, u8 g, u8 b) { SetColor(r, g, b, 0); }
  Color(u8 r, u8 g, u8 b, u8 a) { SetColor(r, g, b, a); }

  // set the color
  // r - red component (0-255)
  // g - green component (0-255)
  // b - blue component (0-255)
  // a - alpha component, controls transparency (0 - transparent, 255 - opaque);
  void SetColor(u8 r, u8 g, u8 b, u8 a = 0) {
    _color[0] = r;
    _color[1] = g;
    _color[2] = b;
    _color[3] = a;
  }

  void GetColor(u8 &r, u8 &g, u8 &b, u8 &a) const {
    r = _color[0];
    g = _color[1];
    b = _color[2];
    a = _color[3];
  }

  void SetRawColor(uint32_t color32) {
    static_assert(sizeof(*this) == sizeof(color32),
                  "Raw color should be same size as *this.");
    memcpy(this, &color32, sizeof(color32));
  }

  uint32_t GetRawColor() const {
    static_assert(sizeof(*this) == sizeof(uint32_t),
                  "Raw color should be same size as *this.");
    return *(reinterpret_cast<const uint32_t *>(this));
  }

  inline u8 r() const { return _color[0]; }
  inline u8 g() const { return _color[1]; }
  inline u8 b() const { return _color[2]; }
  inline u8 a() const { return _color[3]; }

  u8 &operator[](usize index) { return _color[index]; }
  const u8 &operator[](usize index) const { return _color[index]; }

  bool operator==(const Color &rhs) const {
    return memcmp(this, &rhs, sizeof(*this)) == 0;
  }

  bool operator!=(const Color &rhs) const { return !(*this == rhs); }

  Color &operator=(const Color &rhs) {
    SetRawColor(rhs.GetRawColor());
    return *this;
  }

  Color(const Color &rhs) { SetRawColor(rhs.GetRawColor()); }

 private:
  u8 _color[4];  //-V112
};

static_assert(
    std::is_trivial<Color>::value || std::is_standard_layout<Color>::value,
    "Color should be trivial or standard layout to be memset/memcpy-able.");

#endif  // COLOR_H
