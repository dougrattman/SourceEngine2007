// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_POINT_H_
#define SOURCE_VGUI_POINT_H_

#ifdef _WIN32
#pragma once
#endif

#include "vgui/VGUI.h"

namespace vgui {
// Purpose: Basic handler for a Points in 2 dimensions.
class Point {
 public:
  // constructors
  constexpr Point() : x_{0}, y_{0} {}
  constexpr Point(int x, int y) : x_{x}, y_{y} {}

  constexpr void SetPoint(int x, int y) {
    x_ = x;
    y_ = y;
  }

  constexpr void GetPoint(int &x, int &y) const {
    x = x_;
    y = y_;
  }

  constexpr bool operator==(Point &rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_;
  }
  constexpr bool operator!=(Point &rhs) const { return !(*this == rhs); }

 private:
  int x_, y_;
};
}  // namespace vgui

#endif  // SOURCE_VGUI_POINT_H_
