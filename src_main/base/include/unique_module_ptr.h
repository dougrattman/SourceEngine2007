// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_UNIQUE_MODULE_PTR_H_
#define BASE_INCLUDE_UNIQUE_MODULE_PTR_H_

#include <memory>
#include <tuple>
#include <type_traits>

#include "build/include/build_config.h"

#ifdef OS_POSIX
#include <dlfcn.h>  // dlopen, etc.

#include "base/include/posix_errno_info.h"
#endif

#include "base/include/base_types.h"
#include "base/include/check.h"
#include "base/include/compiler_specific.h"
#include "base/include/type_traits.h"

#ifdef OS_WIN
#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"
#endif

namespace source {
#ifdef OS_WIN
// Define DLL module descriptor.
using module_descriptor = HINSTANCE__;
#elif defined(OS_POSIX)
// Define shared library descriptor.
using module_descriptor = void;

// Creates POSIX errno info from dlerror().
posix_errno_info make_posix_dlerror_errno_info() noexcept {
  const char *last_dl_error{dlerror()};
  // TODO(d.rattman): Check dlerror sources to find way to get error code.
  posix_errno_info info = {last_dl_error == nullptr ? EOK : EXIT_FAILURE};

  if (last_dl_error == nullptr) {
    strcpy_s(info.description, "Ok (0)");
    return info;
  }

  const int sprintf_code{
      sprintf_s(info.description, "%s (%d)", last_dl_error, info.code)};

  CHECK(sprintf_code != -1, errno);
  return info;
}
#else  // !OS_WIN && !defined(OS_POSIX)
#error Please add module descriptor support for your platform in base/include/unique_module_ptr.h
#endif  // OS_WIN
}  // namespace source

namespace std {
#ifdef OS_WIN
// Deleter to unload DLL on end of scope.
template <>
struct default_delete<source::module_descriptor> {
  // Use HMODULE here since module_descriptor is HMODULE.
  void operator()(_In_ HMODULE module) const noexcept {
    const DWORD errno_code{FreeLibrary(module) ? NOERROR : GetLastError()};
    CHECK(errno_code == NOERROR, errno_code);
  }
};  // namespace std
#elif defined(OS_POSIX)
// Deleter to unload shared library on end of scope.
template <>
struct default_delete<source::module_descriptor> {
  void operator()(source::module_descriptor *module) const {
    const int dlclose_error_code{dlclose(module)};
    CHECK(dlclose_error_code == 0, dlclose_error_code);
  }
};
#else  // !OS_WIN && !defined(OS_POSIX)
#error Please add module default_delete support for your platform in base/include/unique_module_ptr.h
#endif  // OS_WIN
}  // namespace std

namespace source {
// Smart pointer with std::unique_ptr semantic for module lifecycle handling.
class unique_module_ptr : private std::unique_ptr<module_descriptor> {
  // Define std::unique_ptr members to simplify usage.
  using std::unique_ptr<module_descriptor,
                        std::default_delete<module_descriptor>>::unique_ptr;

 public:
  // Check module loaded like this: if (!module) do_smth.
  using std::unique_ptr<module_descriptor,
                        std::default_delete<module_descriptor>>::operator bool;

#ifdef OS_WIN
  // Loads library |library_name| and gets (unique_module_ptr, errno_info).
  static std::tuple<unique_module_ptr, source::windows::windows_errno_info>
  from_load_library(_In_ const wstr &library_name) noexcept {
    const HMODULE library{LoadLibraryW(library_name.c_str())};
    return {unique_module_ptr{library},
            library != nullptr ? windows::windows_errno_info_ok
                               : windows::windows_errno_info_last_error()};
  }

  // Loads library |library_name| with flags |load_flags| and gets
  // (unique_module_ptr, errno_info).
  static std::tuple<unique_module_ptr, source::windows::windows_errno_info>
  from_load_library(_In_ const wstr &library_name,
                    _In_ u32 load_flags) noexcept {
    const HMODULE library{
        LoadLibraryExW(library_name.c_str(), nullptr, load_flags)};
    return {unique_module_ptr{library},
            library != nullptr ? windows::windows_errno_info_ok
                               : windows::windows_errno_info_last_error()};
  }

  // Gets (address, error_code) of function |function_name| in loaded library
  // module.
  template <typename T>
  std::tuple<T, source::windows::windows_errno_info> get_address_as(
      _In_z_ const ch *function_name) const noexcept {
    static_assert(source::type_traits::is_function_pointer_v<T>,
                  "The T should be a function pointer.");
    const auto address =
        reinterpret_cast<T>(GetProcAddress(get(), function_name));
    return {address, address != nullptr
                         ? windows::windows_errno_info_ok
                         : windows::windows_errno_info_last_error()};
  }
#elif defined(OS_POSIX)
  // Loads shared library |library_name| with flags |load_flags| and get
  // unique_module_ptr to it.
  static std::tuple<unique_module_ptr, posix_errno_info> from_load_library(
      const str &library_name, i32 load_flags) noexcept {
    const void *library{dlopen(library_name.c_str(), load_flags)};
    return {unique_module_ptr{library}, library != nullptr
                                            ? posix_errno_info_ok
                                            : make_posix_dlerror_errno_info()};
  }

  // Gets (address, error_code) of function |function_name| in loaded shared
  // library.
  template <typename T>
  std::tuple<T, posix_errno_info> get_address_as(const ch *function_name) const
      noexcept {
    static_assert(source::type_traits::is_function_pointer<T>::value,
                  "The T should be a function pointer.");
    const auto address = reinterpret_cast<T>(dlsym(get(), function_name));
    return {address, address != nullptr ? posix_errno_info_ok
                                        : make_posix_dlerror_errno_info()};
  }
#else  // !OS_WIN && !defined(OS_POSIX)
#error Please add module default_delete support for your platform in base/include/unique_module_ptr.h
#endif  // OS_WIN
};
}  // namespace source

#endif  // BASE_INCLUDE_UNIQUE_MODULE_PTR_H_
