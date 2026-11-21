#pragma once

#include <memory>

namespace Dal::Detail {
    // MSVC rebinds vector allocators for iterator bookkeeping outside DAL capacity budgets.
    template <class T_> inline constexpr bool IS_ITERATOR_DEBUG_ALLOCATION = false;
#if defined(_MSVC_STL_VERSION) && _ITERATOR_DEBUG_LEVEL != 0
    template <> inline constexpr bool IS_ITERATOR_DEBUG_ALLOCATION<std::_Container_proxy> = true;
#endif
} // namespace Dal::Detail
