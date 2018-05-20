// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_COM_PTR_H_
#define BASE_INCLUDE_WINDOWS_COM_PTR_H_

#include "base/include/windows/windows_light.h"

#include <ObjBase.h>
#include <comip.h>
#include <type_traits>

namespace source::windows {
// COM pointer with automatic IID deducing from Tinterface.
template <typename Tinterface, const IID *TIID = &__uuidof(Tinterface)>
class com_ptr : public _com_ptr_t<_com_IIID<Tinterface, TIID>> {
  static_assert(std::is_abstract_v<Tinterface>,
                "The interface should be abstract.");
  static_assert(std::is_base_of_v<IUnknown, Tinterface>,
                "The interface should derive from IUnknown.");
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_COM_PTR_H_
