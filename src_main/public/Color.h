// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef COLOR_H
#define COLOR_H

#include <memory.h>
#include <cstdint>
#include <type_traits>

// Purpose: Basic handler for an rgb set of colors.
class Color {
 public:
  Color() { memset(this, 0, sizeof(*this)); }
  Color(uint8_t r, uint8_t g, uint8_t b) { SetColor(r, g, b, 0); }
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { SetColor(r, g, b, a); }

  // set the color
  // r - red component (0-255)
  // g - green component (0-255)
  // b - blue component (0-255)
  // a - alpha component, controls transparency (0 - transparent, 255 - opaque);
  void SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0) {
    _color[0] = r;
    _color[1] = g;
    _color[2] = b;
    _color[3] = a;
  }

  void GetColor(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) const {
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

  inline uint8_t r() const { return _color[0]; }
  inline uint8_t g() const { return _color[1]; }
  inline uint8_t b() const { return _color[2]; }
  inline uint8_t a() const { return _color[3]; }

  uint8_t &operator[](size_t index) { return _color[index]; }
  const uint8_t &operator[](size_t index) const { return _color[index]; }

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
  uint8_t _color[4];
};

static_assert(
    std::is_trivial<Color>::value || std::is_standard_layout<Color>::value,
    "Color should be trivial or standard layout to be memset/memcpy-able.");

#endif  // COLOR_H
