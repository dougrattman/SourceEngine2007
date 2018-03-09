// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "base/include/macros.h"

#include <type_traits>

#include "gtest/gtest.h"

TEST(MacrosTest, SOURCE_DECLARE_POINTER_HANDLE_MACRO) {
  SOURCE_DECLARE_POINTER_HANDLE(PointerHandle);

  static_assert(std::is_pointer_v<PointerHandle>, "PointerHandle is pointer.");
  static_assert(std::is_class_v<std::remove_pointer<PointerHandle>>,
                "PointerHandle is pointer to struct/class.");
}

TEST(MacrosTest, SOURCE_FORWARD_DECLARE_HANDLE_MACRO) {
  SOURCE_FORWARD_DECLARE_HANDLE(ForwardHandle);

  static_assert(std::is_pointer_v<ForwardHandle>, "ForwardHandle is pointer.");
  static_assert(std::is_class_v<std::remove_pointer<ForwardHandle>>,
                "ForwardHandle is pointer to struct/class.");
}
