// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef COLOR_H
#define COLOR_H

#include <memory.h>
#include <type_traits>
#include "base/include/base_types.h"
#include "base/include/macros.h"

// Basic handler for an rgba set of colors.
class Color {
 public:
  Color() { memset(this, 0, sizeof(*this)); }
  Color(u8 r, u8 g, u8 b) { SetColor(r, g, b, 0); }
  Color(u8 r, u8 g, u8 b, u8 a) { SetColor(r, g, b, a); }

  // Set the color.
  // r - red   [0-255]
  // g - green [0-255]
  // b - blue  [0-255]
  // a - alpha (0 - transparent, 255 - opaque).
  void SetColor(u8 r, u8 g, u8 b, u8 a = 0) {
    color_[0] = r;
    color_[1] = g;
    color_[2] = b;
    color_[3] = a;
  }

  // r - red   [0-255]
  // g - green [0-255]
  // b - blue  [0-255]
  // a - alpha (0 - transparent, 255 - opaque).
  void GetColor(u8 &r, u8 &g, u8 &b, u8 &a) const {
    r = color_[0];
    g = color_[1];
    b = color_[2];
    a = color_[3];
  }

  void SetRawColor(u32 color32) {
    static_assert(sizeof(*this) == sizeof(color32),
                  "Raw color should be same size as *this.");
    memcpy(this, &color32, sizeof(color32));
  }

  u32 GetRawColor() const {
    static_assert(sizeof(*this) == sizeof(u32),
                  "Raw color should be same size as u32.");
    return *bit_cast<const u32 *>(this);
  }

  inline u8 r() const { return color_[0]; }
  inline u8 g() const { return color_[1]; }
  inline u8 b() const { return color_[2]; }
  inline u8 a() const { return color_[3]; }

  u8 &operator[](usize index) { return color_[index]; }
  const u8 &operator[](usize index) const { return color_[index]; }

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
  u8 color_[4];  //-V112
};

static_assert(
    std::is_trivial<Color>::value || std::is_standard_layout<Color>::value,
    "Color should be trivial or standard layout to be memset/memcpy-able.");

#endif  // COLOR_H
