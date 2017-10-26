// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_UNIQUE_MODULE_PTR_H_
#define BASE_INCLUDE_WINDOWS_UNIQUE_MODULE_PTR_H_

#include <cassert>
#include <memory>
#include <type_traits>

#include "base/include/base_types.h"
#include "base/include/type_traits.h"
#include "base/include/windows/windows_light.h"

namespace source {
namespace windows {
// Define DLL module descriptor.
using module_descriptor = HINSTANCE__;
}  // namespace windows
}  // namespace source

namespace std {
// Deleter to unload DLL on end of scope.
template <>
struct default_delete<source::windows::module_descriptor> {
  // Use HMODULE here to ensure pointer since module_descriptor is HMODULE.
  void operator()(_In_ HMODULE module) const noexcept {
#ifdef _DEBUG
    BOOL result_code =
#endif  // _DEBUG
        FreeLibrary(module);
#ifdef _DEBUG
    assert(result_code != FALSE);
#endif  // _DEBUG
  }
};
}  // namespace std

namespace source {
namespace windows {
// Smart pointer with std::unique_ptr semantic for module lifecycle handling.
class unique_module_ptr : private std::unique_ptr<module_descriptor> {
  // Define std::unique_ptr members to simplify usage.
  using std::unique_ptr<module_descriptor,
                        std::default_delete<module_descriptor>>::unique_ptr;

 public:
  using std::unique_ptr<module_descriptor,
                        std::default_delete<module_descriptor>>::operator bool;

  // Loads library module and get unique_module_ptr to it.
  static unique_module_ptr from_load_library(
      const wstr &library_name) noexcept {
    return unique_module_ptr(LoadLibraryW(library_name.c_str()));
  }

  // Loads library module with flags and get unique_module_ptr to it.
  static unique_module_ptr from_load_library(const wstr &library_name,
                                             u32 load_flags) noexcept {
    return unique_module_ptr(
        LoadLibraryExW(library_name.c_str(), nullptr, load_flags));
  }

  // Gets address of function in loaded library module.
  template <typename T>
  T get_address(_In_z_ const ch *function_name) const noexcept {
    static_assert(type_traits::is_function_pointer<T>::value,
                  "The T should be a function pointer.");
    return reinterpret_cast<T>(GetProcAddress(get(), function_name));
  }
};
}  // namespace windows
}  // namespace source

#endif  // BASE_INCLUDE_WINDOWS_UNIQUE_MODULE_PTR_H_
